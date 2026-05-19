use console::style;

use crate::packets::config::config_keygen::KeyGenConfigPacket;

use super::PHYsecTelemetry;

pub type TelemetryKeyGenConfig = KeyGenConfigPacket;

// PHYsecPayload trait already implemented

impl PHYsecTelemetry for TelemetryKeyGenConfig {
    fn to_display(&self) -> String {
        style(self.to_log()).yellow().bright().to_string()
    }
    fn to_log(&self) -> String {
        let probe_delay_ptr = std::ptr::addr_of!(self.probe_delay);
        let probe_delay = unsafe { probe_delay_ptr.read_unaligned() };
        format!(
            "Keygen ID: {}, Master: {}, CSI Type: {}, Pre-process Type: {}, Quant Type: {}, Recon Type: {}, Probe Delay: {}",
            self.keygen_id, self.is_master, self.csi_type, self.pre_process_type, self.quant_type, self.recon_type, probe_delay
        )
    }
}
