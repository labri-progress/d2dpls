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

    generate_enum_mapping(&bindings_content);

    fs::write(bindings_path, bindings_content).expect("Couldn't write bindings");
}

fn generate_enum_mapping(bindings_content: &str) {
    // here we build the reverse mapping
    let regex_boilerplate = |name: &str| format!(r#"pub const ([A-Z_]+): {name} = (\d+)"#);

    let csi_type_regex = regex_boilerplate("csi_type_t");
    let pre_process_type_regex = regex_boilerplate("preprocess_type_t");
    let quant_type_regex = regex_boilerplate("quant_type_t");
    let recon_type_regex = regex_boilerplate("recon_type_t");

    let mapping_file_path = PathBuf::from("experiment_configuration.md");
    let csi_type_regex = Regex::new(&csi_type_regex).unwrap();
    let pre_process_type_regex = Regex::new(&pre_process_type_regex).unwrap();
    let quant_type_regex = Regex::new(&quant_type_regex).unwrap();
    let recon_type_regex = Regex::new(&recon_type_regex).unwrap();

    let csi_type: String = csi_type_regex
        .captures_iter(bindings_content)
        .map(|v| {
            format!(
                "- `{}`: {}\n",
                v.get(1).unwrap().as_str(),
                v.get(2).unwrap().as_str()
            )
        })
        .collect();

    let pre_process_type: String = pre_process_type_regex
        .captures_iter(bindings_content)
        .map(|v| {
            format!(
                "- `{}`: {}\n",
                v.get(1).unwrap().as_str(),
                v.get(2).unwrap().as_str()
            )
        })
        .collect();

    let quant_type: String = quant_type_regex
        .captures_iter(bindings_content)
        .map(|v| {
            format!(
                "- `{}`: {}\n",
                v.get(1).unwrap().as_str(),
                v.get(2).unwrap().as_str()
            )
        })
        .collect();

    let recon_type: String = recon_type_regex
        .captures_iter(bindings_content)
        .map(|v| {
            format!(
                "- `{}`: {}\n",
                v.get(1).unwrap().as_str(),
                v.get(2).unwrap().as_str()
            )
        })
        .collect();

    let markdown_content = format!(
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

{}

### Supported pre-processing:

Processing method applied to acquisition data before quantization; if any.

`pre_process_type` (uint8)

{}

### Supported quantization methods:

Supported quantization methods, ie. the method used to generate bits from the CSI samples.

`quant_type` (uint8)

{}

### Supported information-reconciliation methods:

`recon_type` (uint8)

{}

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

`tx_power` (uint8)"#,
        csi_type, pre_process_type, quant_type, recon_type
    );

    fs::write(mapping_file_path, markdown_content).expect("Couldn't write mapping");
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
