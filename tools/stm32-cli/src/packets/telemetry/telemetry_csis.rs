use crate::packets::config::config_csis::CSIPacket;

use super::PHYsecTelemetry;

// PHYsecPayload trait already implemented

impl PHYsecTelemetry for CSIPacket {
    fn to_display(&self) -> String {
        self.to_log()
    }
    fn to_log(&self) -> String {
        let mut msg = String::new();
        msg.push_str(&format!("Num CSI: {}\n", self.num_csi));
        msg.push_str("   0:");
        for (i, val) in self.csis.iter().enumerate() {
            let signed_val = *val as i16;
            if i > 0 && i % 8 == 0 {
                msg.push_str(&format!("\n{:4}: {}", i, signed_val));
            } else {
                msg.push_str(&format!(" {}", signed_val));
            }
        }
        msg
    }
}
