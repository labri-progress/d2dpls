use regex::Regex;
use std::fs;
use std::path::PathBuf;

fn main() {
    generate_libphysec_bindings();
    generate_physec_serial_bindings();
    println!("cargo:rerun-if-changed=../../STM32PlatformCode/Firmware/SubGHz_Phy/App/libphysec/libphysec.h");
    println!(
        "cargo:rerun-if-changed=../../STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_serial.h"
    );
}

fn generate_libphysec_bindings() {
    let bindings = bindgen::Builder::default()
        .header("../../STM32PlatformCode/Firmware/SubGHz_Phy/App/libphysec/libphysec.h")
        .ignore_methods()
        .generate()
        .expect("Unable to generate bindings for libphysec");

    let bindings_path = PathBuf::from("src/physec_bindings/libphysec.rs");
    let mut bindings_content = bindings.to_string();

    let re_quant = Regex::new(r"quant_type_t_QUANT").unwrap();
    bindings_content = re_quant.replace_all(&bindings_content, "QUANT").to_string();

    let re_csi = Regex::new(r"csi_type_t_CSI").unwrap();
    bindings_content = re_csi.replace_all(&bindings_content, "CSI").to_string();

    let re_preprocess = Regex::new(r"preprocess_type_t_PREPROCESS").unwrap();
    bindings_content = re_preprocess
        .replace_all(&bindings_content, "PREPROCESS")
        .to_string();

    let re_recon = Regex::new(r"recon_type_t_RECON").unwrap();
    bindings_content = re_recon.replace_all(&bindings_content, "RECON").to_string();

    let re_physec = Regex::new(r"physec_packet_type_t_PHYSEC").unwrap();
    bindings_content = re_physec
        .replace_all(&bindings_content, "PHYSEC")
        .to_string();

    let re_incomplete_array = Regex::new(r"__IncompleteArrayField<u8>").unwrap();
    bindings_content = re_incomplete_array
        .replace_all(&bindings_content, "[u8; 0]")
        .to_string();

    bindings_content = format!("#![allow(warnings)]\n{}", bindings_content);

    generate_experiment_configuration(&bindings_content);

    fs::write(bindings_path, bindings_content).expect("Couldn't write bindings");
}

struct ExperimentConfiguration {
    csi: Vec<(String, String)>,
    pre_process: Vec<(String, String)>,
    quant: Vec<(String, String)>,
    recon: Vec<(String, String)>,
}

macro_rules! enum_member_is_implemented {
    ($enum_value:expr) => {
        if ($enum_value.parse::<u32>().unwrap_or(0) & (1u32 << 7)) == (1 << 7) {
            " (Not Implemented Yet)"
        } else {
            ""
        }
    };
}

macro_rules! binding_type_regex_boilerplate {
    ($name:literal) => {
        concat!(r"pub const ([A-Z_]+): ", $name, r" = (\d+)")
    };
}

macro_rules! binding_type_regex_error {
    ($regex_name:literal) => {
        |_| concat!("Regex Error (", $regex_name, ")")
    };
}

macro_rules! binding_regex_to_values {
    ($regex_source:expr, $binding_content:expr) => {
        $regex_source
            .captures_iter($binding_content)
            .map(|v| {
                (
                    v.get(1).unwrap().as_str().to_string(),
                    v.get(2).unwrap().as_str().to_string(),
                )
            })
            .collect()
    };
}

macro_rules! decompose_enum_kv {
    ($f:expr, $enum_src:expr) => {
        for (k, v) in $enum_src {
            write!($f, "- `{k}`: {v}{}\n", enum_member_is_implemented!(v))?;
        }
    };
}

impl TryFrom<&str> for ExperimentConfiguration {
    type Error = &'static str;

    fn try_from(bindings_content: &str) -> Result<Self, Self::Error> {
        let csi_type_regex = binding_type_regex_boilerplate!("csi_type_t");
        let pre_process_type_regex = binding_type_regex_boilerplate!("preprocess_type_t");
        let quant_type_regex = binding_type_regex_boilerplate!("quant_type_t");
        let recon_type_regex = binding_type_regex_boilerplate!("recon_type_t");

        let csi_type_regex =
            Regex::new(&csi_type_regex).map_err(binding_type_regex_error!("test"))?;
        let pre_process_type_regex = Regex::new(&pre_process_type_regex)
            .map_err(binding_type_regex_error!("pre_process_type"))?;
        let quant_type_regex =
            Regex::new(&quant_type_regex).map_err(binding_type_regex_error!("quant_type"))?;
        let recon_type_regex =
            Regex::new(&recon_type_regex).map_err(binding_type_regex_error!("recon_type"))?;

        let csi: Vec<(String, String)> = binding_regex_to_values!(csi_type_regex, bindings_content);

        let pre_process: Vec<(String, String)> =
            binding_regex_to_values!(pre_process_type_regex, bindings_content);

        let quant: Vec<(String, String)> =
            binding_regex_to_values!(quant_type_regex, bindings_content);

        let recon: Vec<(String, String)> =
            binding_regex_to_values!(recon_type_regex, bindings_content);

        Ok(Self {
            csi,
            pre_process,
            quant,
            recon,
        })
    }
}

impl std::fmt::Display for ExperimentConfiguration {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            r#"# Experiment Config Files

## keygen

All the options related to the keygen pipeline configuration.

### Flashed device role

`is_master` (bool)

### Keygen ID

Identifier of the key-generations. Alice and Bob must use the same Keygen ID in order to be able to run experiments. This number must also be unique among the experiments running in the same time.

`keygen_id` (uint8)

### CSI types:

Method used for acquisition.

`csi_type` (uint8)

"#
        )?;
        decompose_enum_kv!(f, &self.csi);
        write!(
            f,
            r#"
### Supported pre-processing:

Processing method applied to acquisition data before quantization; if any.

`pre_process_type` (uint8)

"#
        )?;
        decompose_enum_kv!(f, &self.pre_process);
        write!(
            f,
            r#"
### Supported quantization methods:

Supported quantization methods, ie. the method used to generate bits from the CSI samples.

`quant_type` (uint8)

"#
        )?;
        decompose_enum_kv!(f, &self.quant);
        write!(
            f,
            r#"
### Supported information-reconciliation methods:

`recon_type` (uint8)

"#
        )?;
        decompose_enum_kv!(f, &self.recon);
        write!(
            f,
            r#"
### Probe Padding

Number of padding bytes in each probe packet.

`probe_padding` (uint8)

### Probe Delay

The delay Alice waits when she receives a reponse to a probe before sending the next probe.

`probe_delay` (uint16)


## Telemetry

All the options related to the telemetry, ie. the verbosity of the experiments.

### Enable Telemetry

If disabled then no logging nor exportation of acquisition / keygen data is done by the device to the host.

`enabled` (bool)

### Enable logging

Enables or disables the logging of messages from the device to the host.

`logging_enabled` (bool)

### Keygen Info

Enables or disables the logging of acquisition / keygen data.

`keygen_info_enabled` (bool)


## Radio

RF parameters.

### Modulation

Used modulation, always LoRa.

`modulation` = "LoRa"

#### Spreading Factor

`spreading_factor` (uint8)

#### Bandwidth

`bandwitdh` (uint8)

#### Transmission Power

`tx_power` (uint8)"#
        )?;
        Ok(())
    }
}

fn generate_experiment_configuration(binding_content: &str) {
    let experiment_configuration = ExperimentConfiguration::try_from(binding_content).unwrap();

    let mapping_file_path = PathBuf::from("experiment_configuration.md");

    fs::write(mapping_file_path, format!("{experiment_configuration}"))
        .expect("Couldn't write mapping");
}

fn generate_physec_serial_bindings() {
    let bindings = bindgen::Builder::default()
        .header("../../STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_serial.h")
        .ignore_methods()
        .clang_arg("-DSTM32L072xx")
        .clang_arg("-I../../STM32PlatformCode/Firmware/Core/Inc/")
        .clang_arg("-I../../STM32PlatformCode/Drivers/STM32L0xx_HAL_Driver/Inc/")
        .clang_arg("-I../../STM32PlatformCode/Firmware/SubGHz_Phy/App")
        .clang_arg("-I../../STM32PlatformCode/Firmware/SubGHz_Phy/Target")
        .clang_arg("-I../../STM32PlatformCode/Firmware/Core/Inc")
        .clang_arg("-I../../STM32PlatformCode/Utilities/misc")
        .clang_arg("-I../../STM32PlatformCode/Utilities/timer")
        .clang_arg("-I../../STM32PlatformCode/Utilities/trace/adv_trace")
        .clang_arg("-I../../STM32PlatformCode/Utilities/lpm/tiny_lpm")
        .clang_arg("-I../../STM32PlatformCode/Utilities/sequencer")
        .clang_arg("-I../../STM32PlatformCode/Drivers/BSP/B-L072Z-LRWAN1")
        .clang_arg("-I../../STM32PlatformCode/Drivers/BSP/CMWX1ZZABZ_0xx")
        .clang_arg("-I../../STM32PlatformCode/Drivers/STM32L0xx_HAL_Driver/Inc")
        .clang_arg("-I../../STM32PlatformCode/Drivers/CMSIS/Device/ST/STM32L0xx/Include")
        .clang_arg("-I../../STM32PlatformCode/Drivers/CMSIS/Include")
        .clang_arg("-I../../STM32PlatformCode/Middlewares/Third_Party/SubGHz_Phy")
        .clang_arg("-I../../STM32PlatformCode/Middlewares/Third_Party/SubGHz_Phy/sx1276")
        .clang_arg("-I../../STM32PlatformCode/Firmware/SubGHz_Phy/App/libphysec")
        .generate()
        .expect("Unable to generate bindings for physec_serial");

    let bindings_path = PathBuf::from("src/physec_bindings/physec_serial.rs");
    let mut bindings_content = bindings.to_string();

    let re_incomplete_array_u8 = Regex::new(r"__IncompleteArrayField<u8>").unwrap();
    bindings_content = re_incomplete_array_u8
        .replace_all(&bindings_content, "[u8; 0]")
        .to_string();

    let re_incomplete_array_i16 = Regex::new(r"__IncompleteArrayField<i16>").unwrap();
    bindings_content = re_incomplete_array_i16
        .replace_all(&bindings_content, "[i16; 0]")
        .to_string();

    let re_phy_layer_radio_config =
        Regex::new(r"physec_physical_layer_config__bindgen_ty_1").unwrap();
    bindings_content = re_phy_layer_radio_config
        .replace_all(&bindings_content, "phy_layer_radio_config")
        .to_string();

    bindings_content = format!("#![allow(warnings)]\n{}", bindings_content);

    fs::write(bindings_path, bindings_content).expect("Couldn't write bindings");
}
