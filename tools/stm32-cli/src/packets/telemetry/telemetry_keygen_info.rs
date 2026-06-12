use console::style;
use nom::{
    bytes::streaming::take,
    number::streaming::{le_u16, le_u8},
    IResult,
};

use crate::{packets::PHYsecPayload, physec_bindings::physec_serial::*};

use super::PHYsecTelemetry;

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PHYsecTelemetryKeyType {
    KeyTypeQuant = KEY_TYPE_QUANT,
    KeyTypePostProcessing = KEY_TYPE_POST_PROCESSING,
    KeyTypeReconciliation = KEY_TYPE_RECONCILIATION,
    KeyTypePrivacyAmplification = KEY_TYPE_PRIVACY_AMPLIFICATION,
}

impl TryFrom<u8> for PHYsecTelemetryKeyType {
    type Error = ();

    fn try_from(v: u8) -> Result<Self, Self::Error> {
        Ok(match v {
            KEY_TYPE_QUANT => PHYsecTelemetryKeyType::KeyTypeQuant,
            KEY_TYPE_POST_PROCESSING => PHYsecTelemetryKeyType::KeyTypePostProcessing,
            KEY_TYPE_RECONCILIATION => PHYsecTelemetryKeyType::KeyTypeReconciliation,
            KEY_TYPE_PRIVACY_AMPLIFICATION => PHYsecTelemetryKeyType::KeyTypePrivacyAmplification,
            _ => return Err(()),
        })
    }
}

#[derive(Debug)]
pub struct TelemetryKeyGenInfo {
    pub key_type: PHYsecTelemetryKeyType,
    pub num_bits: u16,
    pub key: Vec<u8>,
}

impl PHYsecPayload for TelemetryKeyGenInfo {
    fn to_bytes(&self) -> Vec<u8> {
        let mut bytes = Vec::new();
        bytes.push(self.key_type as u8);
        bytes.extend_from_slice(&self.num_bits.to_le_bytes());
        // mask remaining bytes
        let extra_bits = self.num_bits % 8;
        let key = if extra_bits != 0 {
            let mask = !(0xFF << (extra_bits)); // !(v) <=> ~(v) in C
            let mut key = self.key.clone();
            let keylen = key.len();
            key[keylen - 1] &= mask as u8;
            key
        } else {
            self.key.clone()
        };
        bytes.extend_from_slice(&key);
        bytes
    }

    fn from_bytes(input: &[u8]) -> IResult<&[u8], Self> {
        let total_size = input.len();
        let (input, key_type) = le_u8(input).map_err(|e| match e {
            nom::Err::Failure(nom::error::Error {
                input,
                code: nom::error::ErrorKind::Eof,
            })
            | nom::Err::Error(nom::error::Error {
                input,
                code: nom::error::ErrorKind::Eof,
            }) => nom::Err::Incomplete(nom::Needed::new(total_size)),
            other => other,
        })?;
        let key_type = PHYsecTelemetryKeyType::try_from(key_type).map_err(|e| {
            nom::Err::Failure(nom::error::Error::new(input, nom::error::ErrorKind::Verify))
        })?;

        let (input, num_bits) = le_u16(input).map_err(|e| match e {
            nom::Err::Failure(nom::error::Error {
                input,
                code: nom::error::ErrorKind::Eof,
            })
            | nom::Err::Error(nom::error::Error {
                input,
                code: nom::error::ErrorKind::Eof,
            }) => nom::Err::Incomplete(nom::Needed::new(total_size)),
            other => other,
        })?;
        let num_bytes = ((num_bits + 8 - 1) / 8) as u16;
        if num_bytes as usize > input.len() {
            return Err(nom::Err::Failure(nom::error::Error::new(
                input,
                nom::error::ErrorKind::Tag,
            )));
        }
        let (input, key) = take(num_bytes)(input).map_err(|e| match e {
            nom::Err::Failure(nom::error::Error {
                input,
                code: nom::error::ErrorKind::Eof,
            })
            | nom::Err::Error(nom::error::Error {
                input,
                code: nom::error::ErrorKind::Eof,
            }) => nom::Err::Incomplete(nom::Needed::new(total_size)),
            other => other,
        })?;

        // mask remaining bits
        let extra_bits = num_bits % 8;
        let key = if extra_bits != 0 {
            let mask = !(0xFF << (extra_bits)); // !(v) <=> ~(v) in C
            let mut key = key.to_vec();
            let keylen = key.len();
            key[keylen - 1] &= mask as u8;
            key
        } else {
            key.to_vec()
        };

        Ok((
            input,
            TelemetryKeyGenInfo {
                key_type,
                num_bits,
                key,
            },
        ))
    }
}

impl PHYsecTelemetry for TelemetryKeyGenInfo {
    fn to_display(&self) -> String {
        style(self.to_log()).blue().bright().to_string()
    }

    fn to_log(&self) -> String {
        let mut key_name = match self.key_type {
            PHYsecTelemetryKeyType::KeyTypeQuant => "Quant",
            PHYsecTelemetryKeyType::KeyTypePostProcessing => "Post-processing",
            PHYsecTelemetryKeyType::KeyTypeReconciliation => "Reconciliation",
            PHYsecTelemetryKeyType::KeyTypePrivacyAmplification => "Privacy Amplification",
        };
        format!("{} = {:02x?}", format!("{} Key", key_name), self.key)
    }
}
