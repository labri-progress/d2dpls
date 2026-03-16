use dialoguer::FuzzySelect;
use serde::Deserialize;

use crate::config_parse::PLSConfig;
use crate::packets::config::{PHYsecConfigPacket, PHYsecConfigType};
use crate::packets::telemetry::PHYsecTelemetry;
use crate::physec_bindings::physec_serial::PHYSEC_PROBE_DELAY_MAX;
use crate::utils::*;

use crate::packets::config::config_keygen::KeyGenConfigPacket;
use crate::packets::config::config_radio::{
    LoRaRadioBandwidth, LoRaRadioConfig, RadioConfigPacket, RadioModulation,
};
use crate::packets::config::config_telemetry::TelemetryConfigPacket;
use crate::packets::PHYsecPayload;
use crate::physec_bindings::libphysec;
use crate::{packets::config::config_csis::CSIPacket, physec_bindings::libphysec::*};

pub const MENU_IDX_KEYGEN: usize = 0;
pub const MENU_IDX_TELEMETRY: usize = 1;
pub const MENU_IDX_RADIO: usize = 2;
pub const MENU_IDX_LOADCSI: usize = 3;
pub const MENU_IDX_LOADCONFIG: usize = 4;

pub const MAIN_MENU_CHOICES: [&str; 6] = [
    "Configure KeyGen",
    "Configure Telemetry logging",
    "Configure Radio",
    "Load CSI values",
    "Load Paper Scenario config",
    "Exit",
];

pub const MENU_KG_IDX_ROLE: usize = 0;
pub const MENU_KG_IDX_CSI: usize = 1;
pub const MENU_KG_IDX_PREPROCESS: usize = 2;
pub const MENU_KG_IDX_QUANT: usize = 3;
pub const MENU_KG_IDX_RECON: usize = 4;
pub const MENU_KG_IDX_PROBE_PADDING: usize = 5;
pub const MENU_KG_IDX_PROBE_DELAY: usize = 6;

pub const KEYGEN_MENU_CHOICES: [&str; 8] = [
    "Set Role",
    "Set CSI (pRSSI, regRSSI, arRSSI)",
    "Set Pre-Process method",
    "Set Quantization method",
    "Set Reconciliation method",
    "Set Probe padding",
    "Set Probe delay",
    "Finish",
];

pub const MENU_KG_CSI_IDX_PRSSI: usize = 0;
pub const MENU_KG_CSI_IDX_REGRSSI: usize = 1;
pub const MENU_KG_CSI_IDX_ARRSSI: usize = 2;

pub const CSI_MENU_CHOICES: [(&str, csi_type_t); 3] = [
    ("Packet RSSI", CSI_PACKET_RSSI),
    ("Register RSSI", CSI_REGISTER_RSSI),
    ("Adjacent Register RSSI", CSI_ADJACENT_REGISTER_RSSI),
];

pub const MENU_KG_PREPROCESS_IDX_SAVGOLAY: usize = 0;
pub const MENU_KG_PREPROCESS_IDX_KALMAN: usize = 0;
pub const MENU_KG_PREPROCESS_IDX_RANDOMMODEL: usize = 0;

pub const PREPROCESS_MENU_CHOICES: [(&str, preprocess_type_t); 4] = [
    ("No preprocessing", PREPROCESS_NONE),
    ("Savitsky Golay Filtering", PREPROCESS_SAVITSKY_GOLAY),
    ("Kalman Filtering", PREPROCESS_KALMAN),
    ("Random Waypoint Model", PREPROCESS_RANDOM_WAYPOINT_MODEL),
];

pub const QUANT_MENU_CHOICES: [(&str, quant_type_t); 9] = [
    (
        "Multi-Bit Range-based level-crossing (FLoRa)",
        QUANT_MBR_LOSSLESS,
    ),
    (
        "Multi-Bit Entropy-based level-crossing (LoRa-Key)",
        QUANT_MBE_LOSSY,
    ),
    (
        "Lossy Single-Bit Differential level-crossing",
        QUANT_SB_DIFF_LOSSY,
    ),
    (
        "Lossy Single-Bit Excursion level-crossing",
        QUANT_SB_EXCURSION_LOSSY,
    ),
    (
        "Lossy Multi-Bit Excursion level-crossing",
        QUANT_MB_EXCURSION_LOSSY,
    ),
    ("Single-Bit level-crossing", QUANT_SB_LOSSLESS),
    (
        "Single-Bit level-crossing blockwise",
        QUANT_SB_LOSSLESS_BLOCKWISE,
    ),
    ("Lossy Single-Bit level-crossing", QUANT_SB_LOSSY),
    (
        "Lossy Single-Bit level-crossing blockwise",
        QUANT_SB_LOSSY_BLOCKWISE,
    ),
];

pub const MENU_KG_RECON_IDX_FE: usize = 0;
pub const MENU_KG_RECON_IDX_PCS: usize = 1;
pub const MENU_KG_RECON_IDX_ECC_SS: usize = 2;

pub const RECON_MENU_CHOICES: [(&str, recon_type_t); 3] = [
    ("Fuzzy Extractors", RECON_FE_STL),
    ("Perturbed Compressed Sensing", RECON_PCS),
    ("ECC with Secure-Sketch", RECON_ECC_SS),
];

pub const RADIO_MENU_CHOICES: [&str; 1] = ["LoRa"];

pub const MENU_LORA_IDX_SF: usize = 0;
pub const MENU_LORA_IDX_BW: usize = 1;
pub const MENU_LORA_IDX_TXP: usize = 2;

pub const LORA_MENU_CHOICES: [&str; 4] = ["Spreading Factor", "Bandwidth", "TX Power", "Finish"];

pub(crate) fn choose_role() -> bool {
    let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
        .with_prompt("[-] Choose Role")
        .default(0)
        .items(&["Slave", "Master"])
        .interact()
        .unwrap();

    choice != 0
}

pub(crate) fn choose_csi() -> libphysec::csi_type_t {
    let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
        .with_prompt("[-] Choose CSI type")
        .default(0)
        .items(&CSI_MENU_CHOICES.map(|(s, _v)| s)[..])
        .interact()
        .unwrap();

    CSI_MENU_CHOICES[choice].1
}

pub(crate) fn choose_pre_process() -> libphysec::preprocess_type_t {
    let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
        .with_prompt("[-] Choose Pre-Processing method")
        .default(0)
        .items(&PREPROCESS_MENU_CHOICES.map(|(s, _v)| s)[..])
        .interact()
        .unwrap();

    PREPROCESS_MENU_CHOICES[choice].1
}

pub(crate) fn choose_quantization() -> libphysec::quant_type_t {
    let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
        .with_prompt("[-] Choose Quantization method")
        .default(0)
        .items(&QUANT_MENU_CHOICES.map(|(s, _v)| s)[..])
        .interact()
        .unwrap();

    QUANT_MENU_CHOICES[choice].1
}

pub(crate) fn choose_reconciliation() -> libphysec::recon_type_t {
    let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
        .with_prompt("[-] Choose Pre-Processing method")
        .default(0)
        .items(&RECON_MENU_CHOICES.map(|(s, _v)| s)[..])
        .interact()
        .unwrap();

    RECON_MENU_CHOICES[choice].1
}

pub(crate) fn keygen_configuration() -> Option<Vec<PHYsecConfigPacket>> {
    let mut kg_conf = KeyGenConfigPacket::default();
    let mut modified = false;
    loop {
        let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
            .with_prompt("[-] Configure Key Generation parameters")
            .default(0)
            .items(&KEYGEN_MENU_CHOICES[..])
            .interact()
            .unwrap();

        match choice {
            MENU_KG_IDX_ROLE => {
                kg_conf.is_master = choose_role();
            }
            MENU_KG_IDX_CSI => {
                kg_conf.csi_type = choose_csi();
            }
            MENU_KG_IDX_PREPROCESS => {
                kg_conf.pre_process_type = choose_pre_process();
            }
            MENU_KG_IDX_QUANT => {
                kg_conf.quant_type = choose_quantization();
            }
            MENU_KG_IDX_RECON => {
                kg_conf.recon_type = choose_reconciliation();
            }
            MENU_KG_IDX_PROBE_PADDING => {
                let mut padding: i8 = -1;
                while padding < 0 || padding > 100 {
                    padding = dialoguer::Input::<i8>::new()
                        .with_prompt("[-] Enter Probe padding")
                        .interact()
                        .unwrap();
                }
                kg_conf.probe_padding = padding as u8;
            }
            MENU_KG_IDX_PROBE_DELAY => {
                let mut delay: i32 = -1;
                while delay < 0 || delay > PHYSEC_PROBE_DELAY_MAX as i32 {
                    delay = dialoguer::Input::<i32>::new()
                        .with_prompt("[-] Enter Probe delay")
                        .interact()
                        .unwrap();
                }
                kg_conf.probe_delay = delay as u16;
            }
            _ => {
                // TODO: Validate KeyGen config and displays not yet supported methods
                break;
            }
        }
        modified = true;
    }
    if modified {
        Some(vec![PHYsecConfigPacket::new(
            PHYsecConfigType::PHYsecConfigKeyGen,
            Some(Box::new(kg_conf)),
        )])
    } else {
        None
    }
}

pub(crate) fn telemetry_configuration() -> Option<Vec<PHYsecConfigPacket>> {
    let mut tm_conf = TelemetryConfigPacket::default();

    tm_conf.enabled = dialoguer::Confirm::new()
        .with_prompt("[-] Enable Telemetry logging")
        .interact()
        .unwrap();

    if tm_conf.enabled {
        tm_conf.logging_enabled = dialoguer::Confirm::new()
            .with_prompt("[-] Enable log messages")
            .interact()
            .unwrap();

        tm_conf.keygen_info_enabled = dialoguer::Confirm::new()
            .with_prompt("[-] Enable KeyGen telemetry information")
            .interact()
            .unwrap();

        if tm_conf.keygen_info_enabled {
            // TODO: Implement submenu for configuring KeyGen info logging
        }
    }

    Some(vec![PHYsecConfigPacket::new(
        PHYsecConfigType::PHYsecConfigTelemetry,
        Some(Box::new(tm_conf)),
    )])
}

pub(crate) fn radio_configuration() -> Option<Vec<PHYsecConfigPacket>> {
    let modulation = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
        .with_prompt("[-] Choose Radio Modulation")
        .default(0)
        .items(&RADIO_MENU_CHOICES[..])
        .interact()
        .unwrap();

    match RadioModulation::try_from(modulation) {
        Ok(RadioModulation::LoRa) => {
            let mut lora_conf = LoRaRadioConfig::default();
            let mut modified = false;
            loop {
                let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
                    .with_prompt("[-] Configure LoRa parameters")
                    .default(0)
                    .items(&LORA_MENU_CHOICES[..])
                    .interact()
                    .unwrap();

                match choice {
                    MENU_LORA_IDX_SF => {
                        let mut sf = -1;
                        while sf < 6 || sf > 12 {
                            sf = dialoguer::Input::<i8>::new()
                                .with_prompt("[-] Enter Spreading Factor [6-12]")
                                .interact()
                                .unwrap();
                        }
                        lora_conf.spreading_factor = sf as u8;
                    }
                    MENU_LORA_IDX_BW => {
                        let mut bw = -1;
                        while ![125, 250, 500].contains(&bw) {
                            bw = dialoguer::Input::<i16>::new()
                                .with_prompt("[-] Enter Bandwidth (in KHz) [125, 250, 500]")
                                .interact()
                                .unwrap();
                        }
                        lora_conf.bandwidth = match bw {
                            125 => LoRaRadioBandwidth::BW125 as u8,
                            250 => LoRaRadioBandwidth::BW250 as u8,
                            500 => LoRaRadioBandwidth::BW500 as u8,
                            _ => {
                                panic!("hmmm ?");
                            }
                        };
                    }
                    MENU_LORA_IDX_TXP => {
                        let mut txp = -1;
                        while txp < 2 || txp > 17 {
                            txp = dialoguer::Input::<i8>::new()
                                .with_prompt("[-] Enter TX Power (in dBm) [2-17]")
                                .interact()
                                .unwrap();
                        }
                        lora_conf.tx_power = txp as u8;
                    }
                    _ => {
                        break;
                    }
                }
                modified = true;
            }
            if modified {
                return Some(vec![PHYsecConfigPacket::new(
                    PHYsecConfigType::PHYsecConfigRadio,
                    Some(Box::new(RadioConfigPacket::new(
                        RadioModulation::LoRa,
                        Box::new(lora_conf),
                    ))),
                )]);
            }
            None
        }
        _ => None,
    }
}

pub(crate) fn load_csi() -> Option<Vec<PHYsecConfigPacket>> {
    let filename = dialoguer::Input::<String>::new()
        .with_prompt("[-] Enter filename containing CSI measures in binary format")
        .interact()
        .unwrap();

    let file_content = match std::fs::read(&filename) {
        Ok(c) => c,
        Err(e) => {
            perr!("Error reading file {}: {e}", filename);
            return None;
        }
    };
    let payload = match CSIPacket::from_bytes(&file_content) {
        Ok((_remaining, csi)) => {
            println!("{}", csi.to_display());
            csi
        }
        Err(e) => {
            perr!("Error parsing CSI file: {e}");
            return None;
        }
    };
    Some(vec![PHYsecConfigPacket::new(
        PHYsecConfigType::PHYsecConfigLoadCSIs,
        Some(Box::new(payload)),
    )])
}

pub(crate) fn load_config_file(config_dir: &str) -> Option<Vec<PHYsecConfigPacket>> {
    let mut configs = Vec::new();
    let mut menu_options = Vec::new();
    for entry in std::fs::read_dir(config_dir).unwrap() {
        let entry = entry.unwrap();
        let filepath = entry.path();
        if filepath.extension().expect("Couldn't find config files") != "toml" {
            continue;
        }

        let config = PLSConfig::from_file(filepath.to_str().unwrap());

        let opt_name = match config.metadata.as_ref() {
            Some(m) => {
                if let Some(alias) = &m.title_alias {
                    alias.clone()
                } else {
                    m.title.clone()
                }
            }
            None => entry
                .path()
                .file_name()
                .unwrap()
                .to_str()
                .unwrap()
                .to_string(),
        };
        let role = if config.is_master_config() { "M" } else { "S" };
        let opt_name = format!("({}) {}", role, opt_name);
        menu_options.push(opt_name);
        configs.push(config);
    }

    let mut combined: Vec<(&String, PLSConfig)> = menu_options
        .iter()
        .zip(configs.into_iter())
        .collect::<Vec<(&String, PLSConfig)>>();

    combined.sort_by(|a, b| a.0[4..].cmp(&b.0[4..])); // discard role from comparison

    let (menu_options, configs): (Vec<&String>, Vec<PLSConfig>) = combined.into_iter().unzip();

    let mut menu_options: Vec<&str> = menu_options.iter().map(|s| s.as_str()).collect();
    menu_options.push("Exit");

    let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
        .with_prompt("[-] Select Scenario configuration (M=Master, S=Slave)")
        .default(0)
        .items(&menu_options[..])
        .interact()
        .unwrap();

    if choice == configs.len() {
        // exit
        return None;
    }

    Some(configs[choice].get_config_packets())
}
