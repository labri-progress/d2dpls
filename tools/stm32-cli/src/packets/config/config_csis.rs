use nom::{
    number::streaming::{le_i16, le_u32},
    IResult,
};

use super::PHYsecPayload;

#[derive(Debug)]
pub struct CSIPacket {
    pub num_csi: u32,
    pub csis: Vec<i16>,
}

impl PHYsecPayload for CSIPacket {
    fn to_bytes(&self) -> Vec<u8> {
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&self.num_csi.to_le_bytes());
        for csi in &self.csis {
            bytes.extend_from_slice(&csi.to_le_bytes());
        }
        bytes
    }

    fn from_bytes(input: &[u8]) -> IResult<&[u8], Self>
    where
        Self: Sized,
    {
        let total_size = input.len();

        let (input, num_csi) = le_u32(input).map_err(|e| match e {
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
        let mut csis = Vec::new();
        let mut ptr: &[u8] = &input[..];
        for i in 0..num_csi {
            let (rem, csi) = le_i16(ptr).map_err(|e| match e {
                nom::Err::Failure(nom::error::Error {
                    input,
                    code: nom::error::ErrorKind::Eof,
                })
                | nom::Err::Error(nom::error::Error {
                    input,
                    code: nom::error::ErrorKind::Eof,
                }) => {
                    //let needed_size = num_csi as usize * std::mem::size_of::<i16>()
                    //    - csis.len() * std::mem::size_of::<i16>();
                    nom::Err::Incomplete(nom::Needed::new(total_size))
                }
                other => other,
            })?;
            csis.push(csi);
            ptr = rem;
        }
        Ok((ptr, CSIPacket { num_csi, csis }))
    }
}
