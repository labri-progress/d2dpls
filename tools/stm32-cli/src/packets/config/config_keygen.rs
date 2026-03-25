use nom::{bytes::streaming::take, number::streaming::le_u16, IResult};
use serde::Deserialize;

use crate::physec_bindings::libphysec::{
    CSI_PACKET_RSSI, PREPROCESS_SAVITSKY_GOLAY, QUANT_MBR_LOSSLESS, RECON_ECC_SS, 
};
use crate::physec_bindings::physec_serial::{DEFAULT_KEYGEN_ID, PHYSEC_PROBE_DELAY, PHYSEC_PROBE_PADDING};

use super::PHYsecPayload;

#[repr(C, packed)]
#[derive(Debug, Deserialize, Clone)]
pub struct KeyGenConfigPacket {
    pub is_master: bool,
    pub keygen_id: u8,
    pub csi_type: u8,
    pub pre_process_type: u8,
    pub quant_type: u8,
    pub recon_type: u8,
    pub probe_padding: u8,
    pub probe_delay: u16,
}

impl KeyGenConfigPacket {
    pub fn new(
        is_master: bool,
        keygen_id: u8,
        csi_type: u8,
        pre_process_type: u8,
        quant_type: u8,
        recon_type: u8,
        probe_padding: u8,
        probe_delay: u16,
    ) -> Self {
        Self {
            is_master,
            keygen_id,
            csi_type,
            pre_process_type,
            quant_type,
            recon_type,
            probe_padding,
            probe_delay,
        }
    }

    pub fn mem_size() -> usize {
        std::mem::size_of::<KeyGenConfigPacket>()
    }
}

impl Default for KeyGenConfigPacket {
    fn default() -> Self {
        Self {
            is_master: false,
            keygen_id: DEFAULT_KEYGEN_ID,
            csi_type: CSI_PACKET_RSSI,
            pre_process_type: PREPROCESS_SAVITSKY_GOLAY,
            quant_type: QUANT_MBR_LOSSLESS,
            recon_type: RECON_ECC_SS,
            probe_padding: PHYSEC_PROBE_PADDING,
            probe_delay: PHYSEC_PROBE_DELAY as u16,
        }
    }
}

impl PHYsecPayload for KeyGenConfigPacket {
    fn to_bytes(&self) -> Vec<u8> {
        [vec![
            self.is_master as u8,
            self.keygen_id,
            self.csi_type,
            self.pre_process_type,
            self.quant_type,
            self.recon_type,
            self.probe_padding,
        ],
        self.probe_delay.to_le_bytes().to_vec()].concat().to_vec()
    }

    fn from_bytes(input: &[u8]) -> IResult<&[u8], Self> {
        let total_size = input.len();
        let (input, is_master) = take(1usize)(input).map_err(|e| match e {
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
        let (input, keygen_id) = take(1usize)(input).map_err(|e| match e {
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
        let (input, csi_type) = take(1usize)(input).map_err(|e| match e {
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

        let (input, pre_process_type) = take(1usize)(input).map_err(|e| match e {
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

        let (input, quant_type) = take(1usize)(input).map_err(|e| match e {
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

        let (input, recon_type) = take(1usize)(input).map_err(|e| match e {
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
        let (input, probe_padding) = take(1usize)(input).map_err(|e| match e {
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
        let (input, probe_delay) = le_u16(input).map_err(|e| match e {
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

        Ok((
            input,
            KeyGenConfigPacket {
                is_master: is_master[0] != 0,
                keygen_id: keygen_id[0],
                csi_type: csi_type[0],
                pre_process_type: pre_process_type[0],
                quant_type: quant_type[0],
                recon_type: recon_type[0],
                probe_padding: probe_padding[0],
                probe_delay,
            },
        ))
    }
}
