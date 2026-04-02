use config::{PHYsecConfigPacket, PHYsecConfigType};
use lazy_static::lazy_static;
use nom::IResult;
use std::fmt::Debug;

use crate::physec_bindings::physec_serial::phy_layer_radio_config;

pub mod config;
pub mod telemetry;

lazy_static! {
    pub static ref PACKET_CONF_DONE: Vec<u8> = {
        let p = PHYsecConfigPacket::new(PHYsecConfigType::PHYsecConfigDone, None);
        p.to_bytes()
    };
    pub static ref PACKET_CONF_START: Vec<u8> = {
        let p = PHYsecConfigPacket::new(PHYsecConfigType::PHYsecConfigStart, None);
        p.to_bytes()
    };
    pub static ref RADIO_CONFIG_UNION_SIZE: usize =
        { std::mem::size_of::<phy_layer_radio_config>() };
}

/// Trait for representing a variable length payload.
/// The Packet has a header with a type in C which helps
/// determining the payload size.
/// This helps harmnozing the serializing/deserializing
/// of main packets `PHYsecConfigPacket` and `PHYsecTelemetryPacket`
pub trait PHYsecPayload: Debug {
    fn to_bytes(&self) -> Vec<u8>;
    fn from_bytes(input: &[u8]) -> IResult<&[u8], Self>
    where
        Self: Sized;
}
