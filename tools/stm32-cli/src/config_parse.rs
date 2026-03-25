use serde::de::IntoDeserializer;
use serde::de::{self, MapAccess, Visitor};
use serde::{Deserialize, Deserializer};
use serde_value::Value as SerdeValue;

use crate::packets::config::config_keygen::KeyGenConfigPacket;
use crate::packets::config::config_radio::{
    LoRaRadioBandwidth, LoRaRadioConfig, RadioConfigPacket, RadioModulation,
};
use crate::packets::config::config_telemetry::TelemetryConfigPacket;
use crate::packets::config::{PHYsecConfigPacket, PHYsecConfigType};
use crate::packets::PHYsecPayload;

#[derive(Debug, Deserialize)]
pub struct PaperMetadata {
    pub title: String,
    pub title_alias: Option<String>,
    pub doi: Option<String>,
    pub link: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct PLSConfig {
    pub metadata: Option<PaperMetadata>,
    keygen: KeyGenConfigPacket,
    telemetry: TelemetryConfigPacket,
    radio: RadioConfigPacket,
}

impl PLSConfig {
    pub fn from_file(config_file: &str) -> Result<Self, String> {
        let file_content = std::fs::read_to_string(config_file).unwrap();

        match toml::from_str(&file_content) {
            Ok(c) => Ok(c),
            Err(e) => Err(format!("Error parsing config file `{}`: {}", config_file, e)),
        }
    }

    pub fn get_config_packets(&self) -> Vec<PHYsecConfigPacket> {
        let kg_pkt = PHYsecConfigPacket::new(
            PHYsecConfigType::PHYsecConfigKeyGen,
            Some(Box::new(self.keygen.clone())),
        );
        let telemetry_pkt = PHYsecConfigPacket::new(
            PHYsecConfigType::PHYsecConfigTelemetry,
            Some(Box::new(self.telemetry.clone())),
        );
        let radio_conf = RadioConfigPacket::from_bytes(self.radio.to_bytes().as_slice())
            .unwrap()
            .1;
        let radio_pkt = PHYsecConfigPacket::new(
            PHYsecConfigType::PHYsecConfigRadio,
            Some(Box::new(radio_conf)),
        );

        let mut packets = Vec::<PHYsecConfigPacket>::new();
        packets.push(telemetry_pkt);
        packets.push(kg_pkt);
        packets.push(radio_pkt);
        packets
    }

    pub fn is_master_config(&self) -> bool {
        self.keygen.is_master
    }
}

/// Returns a list of packets to send for configuring PHYsec on the STM32 board
/// according to the config file
pub fn process_config_file(config_file: &str) -> Vec<PHYsecConfigPacket> {
    let config = PLSConfig::from_file(config_file).unwrap();
    let kg_pkt = PHYsecConfigPacket::new(
        PHYsecConfigType::PHYsecConfigKeyGen,
        Some(Box::new(config.keygen.clone())),
    );
    let telemetry_pkt = PHYsecConfigPacket::new(
        PHYsecConfigType::PHYsecConfigTelemetry,
        Some(Box::new(config.telemetry.clone())),
    );
    let radio_pkt = PHYsecConfigPacket::new(
        PHYsecConfigType::PHYsecConfigRadio,
        Some(Box::new(config.radio)),
    );

    let mut packets = Vec::<PHYsecConfigPacket>::new();
    packets.push(telemetry_pkt);
    packets.push(kg_pkt);
    packets.push(radio_pkt);
    packets
}

// This is needed because the RadioConfigPacket struct holds a dynamic trait object.
// Thus, we need to be able to parse variants. It also enable finer control over the
// parsing process.
// You could also implement such deserialization code for the other PLSConfig
// fields (for now, we derive the `Deserialize` trait for them, but it doesn't check
// for validity of data). It helps simplifying the config parsing but assume you
// provide a valid config file.
impl<'de> Deserialize<'de> for RadioConfigPacket {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct RadioConfigPacketVisitor;

        impl<'de> Visitor<'de> for RadioConfigPacketVisitor {
            type Value = RadioConfigPacket;

            fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                formatter.write_str("struct RadioConfigPacket")
            }

            fn visit_map<V>(self, mut map: V) -> Result<RadioConfigPacket, V::Error>
            where
                V: MapAccess<'de>,
            {
                let mut modulation: Option<RadioModulation> = None;
                let mut radio_config_value: Option<SerdeValue> = None;

                while let Some(key) = map.next_key::<String>()? {
                    match key.as_str() {
                        "modulation" => {
                            if modulation.is_some() {
                                return Err(de::Error::duplicate_field("modulation"));
                            }
                            modulation = Some(map.next_value()?);
                        }
                        "radio_config" => {
                            if radio_config_value.is_some() {
                                return Err(de::Error::duplicate_field("radio_config"));
                            }
                            radio_config_value = Some(map.next_value()?);
                        }
                        _ => {
                            let _ = map.next_value::<de::IgnoredAny>()?;
                        }
                    }
                }

                let modulation =
                    modulation.ok_or_else(|| de::Error::missing_field("modulation"))?;
                let radio_config_value =
                    radio_config_value.ok_or_else(|| de::Error::missing_field("radio_config"))?;

                let radio_config: Box<dyn PHYsecPayload> = match modulation {
                    RadioModulation::LoRa => {
                        let mut deserializer = radio_config_value.into_deserializer();
                        let mut config = LoRaRadioConfig::deserialize(deserializer)
                            .map_err(de::Error::custom)?;
                        if config.spreading_factor < 7 || config.spreading_factor > 12 {
                            return Err(de::Error::custom(
                                "Spreading factor must be between 7 and 12",
                            ));
                        }

                        if ![125u16, 250, 500].contains(&(config.bandwidth as u16)) {
                            return Err(de::Error::custom("Bandwidth must be 125, 250, or 500"));
                        }

                        config.bandwidth = match config.bandwidth as u16 {
                            125 => LoRaRadioBandwidth::BW125 as u8,
                            250 => LoRaRadioBandwidth::BW250 as u8,
                            500 => LoRaRadioBandwidth::BW500 as u8,
                            _ => {
                                return Err(de::Error::custom(
                                    "Bandwidth must be 125, 250, or 500",
                                ));
                            }
                        };

                        if config.tx_power < 2 || config.tx_power > 17 {
                            return Err(de::Error::custom("TX power must be between 2 and 17"));
                        }

                        Box::new(config)
                    }
                    RadioModulation::FSK => {
                        unimplemented!("PHYsec is not implemented yet for FSK modulation")
                    }
                };

                Ok(RadioConfigPacket {
                    modulation,
                    radio_config,
                })
            }
        }

        deserializer.deserialize_map(RadioConfigPacketVisitor)
    }
}
