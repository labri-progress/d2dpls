#![allow(warnings)]
pub mod config_parse;
pub mod menus;
pub mod packets;
pub mod physec_bindings;
pub mod utils;

use menus::{
    keygen_configuration, load_csi, radio_configuration, telemetry_configuration, MAIN_MENU_CHOICES,
};

use std::{
    any::Any,
    env::{self, current_exe},
    fmt::{format, Display, Error},
    fs::{self, File, OpenOptions},
    io::{Read, Seek, Write},
    os::unix::fs::MetadataExt,
    panic::{self, PanicInfo},
    path::{Path, PathBuf},
    thread::sleep,
    time::Duration,
};

use console::style;
use dialoguer::FuzzySelect;
use nom::{Err::Failure, Finish, Needed};
use physec_bindings::libphysec::{
    csi_type_t, preprocess_type_t, quant_type_t, recon_type_t, CSI_ADJACENT_REGISTER_RSSI,
    CSI_PACKET_RSSI, CSI_REGISTER_RSSI, PREPROCESS_KALMAN, PREPROCESS_RANDOM_WAYPOINT_MODEL,
    PREPROCESS_SAVITSKY_GOLAY, QUANT_MBE_LOSSY, QUANT_MBR_LOSSLESS, QUANT_MB_EXCURSION_LOSSY,
    QUANT_SB_DIFF_LOSSY, QUANT_SB_EXCURSION_LOSSY, RECON_ECC_SS, RECON_FE_STL, RECON_PCS,
};
use probe_rs::{
    probe::{list::Lister, Probe},
    Permissions,
};

use crate::packets::config::config_csis::CSIPacket;
use packets::{
    config::{
        config_keygen::KeyGenConfigPacket,
        config_radio::{LoRaRadioBandwidth, LoRaRadioConfig, RadioConfigPacket, RadioModulation},
        config_telemetry::TelemetryConfigPacket,
        PHYsecConfigPacket, PHYsecConfigType,
    },
    telemetry::{PHYsecTelemetry, PHYsecTelemetryPacket, PHYsecTelemetryType},
    PHYsecPayload,
};
use packets::{PACKET_CONF_DONE, PACKET_CONF_START};
use udev::Enumerator;

use menus::*;

use clap::Parser;

use serialport::{ClearBuffer, SerialPortInfo, SerialPortType, UsbPortInfo};

use crate::utils::*;

const STLINK_VID: u16 = 0x0483;
const STLINK_PIDS: [u16; 8] = [
    0x347bu16, // STLINK V2-1
    0x3744,    // STLINK V1
    0x3748,    // STLINK V2
    0x374b,    // STLINK V2-1
    0x3752,    // STLINK V2-1
    0x374e,    // STLINK V3
    0x374f,    // STLINK V3
    0x3753,    // STLINK V3
];

#[derive(Debug)]
enum KeygenRole {
    Alice,
    Bob,
}

impl Display for KeygenRole {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let role_str = match self {
            KeygenRole::Alice => "alice",
            KeygenRole::Bob => "bob",
        };
        write!(f, "{role_str}")
    }
}

fn reset_device(serial: &str) {
    let lister = Lister::new();
    let probes = lister.list_all();
    let probe = match probes.iter().find(|p| format!("{p}").contains(serial)) {
        Some(p) => p,
        None => {
            perr!("No STLINK probe found");
            std::process::exit(1);
        }
    };

    let mut session = probe
        .open()
        .unwrap()
        .attach_under_reset("STM32L072CZYx", Permissions::default())
        .unwrap();

    let mut core = session.core(0).unwrap();

    core.reset().unwrap();
}

#[derive(Parser)]
#[command(version, about = "Interact with STM32 PHYsec boards (Configuration & Logging)", long_about = None)]
struct Args {
    #[clap(
        short,
        long,
        help = "Baudrate for serial connection",
        default_value_t = 115_200
    )]
    baudrate: u32,
    #[clap(short, long, help = "Serial port to use")]
    port: Option<PathBuf>,
    #[clap(
        short,
        long,
        help = "File containing PHYsec configuration to send to STM32"
    )]
    config_file: Option<String>,
    #[clap(
        short = 'C',
        long,
        help = "Directory containing PHYsec configuration files",
        required = false
    )]
    config_dir: Option<String>,
    #[clap(
        short,
        long,
        default_value_t = true,
        help = "Start listening to Telemetry event (after configuration)"
    )]
    listen: bool,
    #[clap(
        short,
        long,
        default_value_t = false,
        help = "Skip PHYsec configuration"
    )]
    skip_config: bool,
    #[clap(
        short = 'S',
        long,
        default_value_t = false,
        help = "Save telemetry data to file"
    )]
    save: bool,
    #[clap(
        short = 'R',
        long,
        default_value_t = false,
        help = "Reset STM32 board before communication"
    )]
    reset: bool,
    #[clap(
        short = 'm',
        long,
        help = "Provide CSI measurements (in custom binary format)"
    )]
    csi_filename: Option<String>,
}

fn main() {
    let args = Args::parse();

    //panic::set_hook(Box::new(|info| {
    //    perr!("Oop ! It seems something went wrong 😱");
    //    std::process::abort()
    //}));

    ctrlc::set_handler(move || {
        println!();
        plog!("Good by(t)e ! 😄");
        std::process::exit(0);
    })
    .expect("Error setting Ctrl-C handler");

    let ports = match serialport::available_ports() {
        Ok(ports) => ports,
        Err(e) => {
            perr!("No serial ports found: {e}");
            std::process::exit(1);
        }
    };

    let mut no_serial = false;
    let mut stlinks: Vec<(SerialPortInfo, String)> = ports
        .into_iter()
        .filter_map(|p| {
            if let SerialPortType::UsbPort(ref usb_info) = p.port_type.clone() {
                println!("{:x}:{:x}", usb_info.vid, usb_info.pid);
                if usb_info.vid == STLINK_VID && STLINK_PIDS.contains(&usb_info.pid) {
                    // Return the SerialPortInfo and Serial Number as a tuple
                    if let Some(serial_number) = &usb_info.serial_number {
                        return Some((p, serial_number.clone()));
                    } else {
                        no_serial = true;
                        return Some((p, "Unknown".to_string()));
                    }
                }
            }
            None
        })
        .collect();

    if no_serial {
        if stlinks.len() > 1 {
            perr!("Cannot retrieve serial number from STLINK. Please unplug all other STM32 if running the tool from docker");
            std::process::exit(1);
        }

        for probe in Lister::new().list_all() {
            if probe.vendor_id == STLINK_VID && STLINK_PIDS.contains(&probe.product_id) {
                if let Some(serial_number) = probe.serial_number {
                    match stlinks.first_mut() {
                        Some((p, s)) => {
                            *s = serial_number;
                        }
                        None => {}
                    }
                    break;
                }
            }
        }
    }

    // STM32 Board selection
    let stm32 = match stlinks.len() {
        0 => {
            perr!("No STLINK found (verify the STM32 board is connected)");
            return;
        }
        l if l >= 2 => {
            let choices = stlinks
                .iter()
                .map(|(p, s)| format!("{} -- {}", p.port_name, s))
                .collect::<Vec<String>>();
            let selection = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
                .with_prompt("[+] Select ST-LINK")
                .default(0)
                .items(&choices[..])
                .interact()
                .unwrap();
            stlinks[selection].clone()
        }
        _ => {
            let stlink = stlinks[0].clone();
            pinfo!("Using ST-LINK: {}", stlink.0.port_name);
            stlink
        }
    };

    pinfo!("Opening {}", stm32.0.port_name);

    let mut serial = match serialport::new(&stm32.0.port_name, args.baudrate)
        .timeout(Duration::from_millis(10))
        .open()
    {
        Ok(p) => p,
        Err(e) => {
            perr!("Unable to open serial port {}: {e}", stm32.0.port_name);
            std::process::exit(1);
        }
    };

    if args.reset {
        // use probe-rs to reset the board
        pinfo!("Resetting device ...");
        reset_device(&stm32.1);
        let _ = serial.clear(ClearBuffer::All).map_err(|e| {
            perr!("Unable to clear I/O streams: {e}");
            std::process::exit(1);
        });
        sleep(Duration::from_millis(1000));
        pinfo!("Device reset");
    }

    if args.config_file.is_some() || !args.skip_config {
        match serial.write_all(&PACKET_CONF_START) {
            Ok(_) => {
                serial.flush().unwrap();
                pinfo!("Configuration started.");
                sleep(Duration::from_millis(250));
            }
            Err(e) => {
                perr!("Could not send Configuration Start packet: {e}");
                std::process::exit(1);
            }
        };
    }

    if let Some(config_file) = &args.config_file {
        pinfo!(
            "Writing file config \"{}\" to {}",
            config_file,
            stm32.0.port_name
        );
        for packet in config_parse::process_config_file(config_file) {
            plog!("Sending {:#?} packet", style(packet.config_type).bold());
            println!("{:#?}", packet);
            //println!("{:02x?}", &packet.to_bytes()[..]);
            match serial.write_all(&packet.to_bytes()) {
                Ok(_) => {
                    serial.flush().unwrap();
                    sleep(Duration::from_millis(250));
                    let mut buf = [0u8; 128];
                    if let Ok(n) = serial.read(&mut buf) {
                        println!("{:02x?}", &buf[..n]);
                    }
                }
                Err(e) => {
                    perr!("Error writing packet to UART: {e}");
                    std::process::exit(1);
                }
            }
        }
    } else if !args.skip_config {
        loop {
            let choice = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
                .with_prompt("[-] Select Option")
                .default(0)
                .items(&MAIN_MENU_CHOICES[..])
                .interact()
                .unwrap();
            // === Finding where lies config_files
            // the issue is that using "./config_files" means if started with the symlink then
            // "./config_files" does not exist, thus causing a panic!
            // easiest way to fix that was to get the executable absolute path and navigate to
            // config_files, assuming nothing was changed in the path (if the user decided to
            // change paths, we assume he can use -C ^^)

            let config_dir: Result<String, String> = match args.config_dir {
                Some(ref cfg) => Ok(cfg.to_string()),
                None => (|| -> Result<String, String> {
                    let current_exe = std::env::current_exe().expect(
                        "failed to retrieve current executable name. this is probably an OS issue",
                    );

                    let current_exe_path = current_exe
                        .parent()
                        .ok_or_else(|| "failed to get parent directory".to_string())?
                        .parent()
                        .ok_or_else(|| "failed to get grandparent directory".to_string())?
                        .parent()
                        .ok_or_else(|| "failed to get great-grandparent directory".to_string())?;

                    let dir_entries = current_exe_path
                        .read_dir()
                        .map_err(|e| format!("failed to open directory: {}", e))?;

                    for entry_result in dir_entries {
                        let entry = entry_result
                            .map_err(|e| format!("error reading directory entry: {}", e))?;

                        if entry.file_name() == "config_files" {
                            return Ok(entry.path().to_string_lossy().into_owned());
                        }
                    }

                    Err("Couldn't find default config dir".to_string())
                })(),
            };
            let config_dir = config_dir.unwrap();

            let packets = match choice {
                MENU_IDX_KEYGEN => keygen_configuration(),
                MENU_IDX_TELEMETRY => telemetry_configuration(),
                MENU_IDX_RADIO => radio_configuration(),
                MENU_IDX_LOADCSI => load_csi(),
                MENU_IDX_LOADCONFIG => load_config_file(&config_dir),
                _ => {
                    break;
                }
            };
            if let Some(packets) = packets {
                for packet in packets {
                    plog!("Sending {:#?} packet", style(packet.config_type).bold());
                    let mut packet_bytes = packet.to_bytes();

                    println!("{:02x?}", packet_bytes);
                    if packet.config_type == PHYsecConfigType::PHYsecConfigLoadCSIs {
                        std::fs::write("csi.bin", &packet_bytes).unwrap();
                    }

                    match serial.write_all(&packet.to_bytes()) {
                        Ok(_) => {
                            serial.flush().unwrap();
                            sleep(Duration::from_millis(250));
                            //let mut buf = [0u8; 512];
                            //match serial.read(&mut buf[..]) {
                            //    Ok(n) => { println!("{:02x?}", &buf[..n]); }
                            //    Err(_) => {
                            //        perr!("STM32 closed configuration time window. Reset the Board !");
                            //        std::process::exit(1);
                            //    }
                            //}
                        }
                        Err(e) => {
                            perr!("Error writing packet to UART: {e}");
                            std::process::exit(1);
                        }
                    }
                }
            }
        }
    }

    if let Some(csi_filename) = args.csi_filename {
        let csi_bytes = std::fs::read(&csi_filename).unwrap();
        let csis = match CSIPacket::from_bytes(&csi_bytes) {
            Ok((_remaining, csi)) => csi,
            Err(e) => {
                perr!("Error parsing CSI file: {e}");
                std::process::exit(1);
            }
        };

        let pkt =
            PHYsecConfigPacket::new(PHYsecConfigType::PHYsecConfigLoadCSIs, Some(Box::new(csis)));
        let mut packet_bytes = pkt.to_bytes();
        match serial.write_all(&packet_bytes) {
            Ok(_) => {
                serial.flush().unwrap();
                pinfo!("CSI values loaded");
            }
            Err(e) => {
                perr!("Error writing packet to UART: {e}");
                std::process::exit(1);
            }
        }
    }

    sleep(Duration::from_millis(250));
    serial.clear(ClearBuffer::All).unwrap();
    match serial.write_all(&PACKET_CONF_DONE) {
        Ok(_) => {
            serial.flush().unwrap();
            pinfo!("Configuration done");
        }
        Err(e) => {
            perr!("Could not send ConfigDone packet: {e}");
            std::process::exit(1);
        }
    };

    sleep(Duration::from_millis(250));

    let timestamp_start = chrono::Local::now().format("%Y-%m-%d_%H-%M-%S");
    let mut logfile_name = format!("log_{}.txt", timestamp_start);
    if args.listen {
        pinfo!("{}", style("Listening for telemetry data").green().bold());

        let mut logfile = if args.save {
            Some(
                OpenOptions::new()
                    .write(true)
                    .read(true) // because we wanna be able to copy its content
                    // later for changing log file on the fly
                    .create(true)
                    .open(&logfile_name)
                    .unwrap(),
            )
        } else {
            None
        };

        // start waiting for telemetry data to display
        let mut rx_buf = [0u8; 8096];
        let mut cur_size: usize = 0;

        // if keygen config packet detected, will be set to either alice or bob in order to help
        // naming log and csi files !
        // this will be set only if args.save is true btw
        let mut role: Option<KeygenRole> = None;

        'tm_rx_loop: loop {
            if let Ok(n) = serial.read(&mut rx_buf[cur_size..]) {
                //println!("{:02x?}", &rx_buf[..n+cur_size]);
                cur_size += n;
                if cur_size >= rx_buf.len() {
                    //println!("{:02x?}", &rx_buf[..cur_size]);
                    perr!("Buffer overflow");
                    break;
                }
                while let Ok((remaining, packet)) =
                    PHYsecTelemetryPacket::from_bytes(&rx_buf[..cur_size].to_vec().clone()[..])
                {
                    println!("{}", packet.to_display());

                    if args.save {
                        if packet.payload.is_none() {
                            continue;
                        }
                        let cur_time = chrono::Local::now().format("%Y-%m-%d_%H-%M-%S");

                        let payload = packet.payload.as_ref().unwrap();

                        match packet.telemetry_type {
                            packets::telemetry::PHYsecTelemetryType::PHYsecTelemetryCSIs => {
                                let csi_file_format = if let Some(role) = &role {
                                    format!("csi_{}_{}.bin", role, cur_time)
                                } else {
                                    format!("csi_{}.bin", cur_time)
                                };
                                match OpenOptions::new()
                                    .write(true)
                                    .create(true)
                                    .open(&csi_file_format)
                                {
                                    Ok(mut f) => {
                                        f.write_all(&payload.to_bytes()).unwrap();
                                    }
                                    Err(e) => {
                                        perr!("Error opening file for writing: {e}");
                                    }
                                }
                                if let Some(role) = &role {
                                    logfile
                                        .as_mut()
                                        .unwrap()
                                        .write_all(
                                            &format!("[csi_{}_{}.bin]\n", role, cur_time)
                                                .as_bytes(),
                                        )
                                        .unwrap();
                                } else {
                                    logfile
                                        .as_mut()
                                        .unwrap()
                                        .write_all(&format!("[csi_{}.bin]\n", cur_time).as_bytes())
                                        .unwrap();
                                }
                            }
                            packets::telemetry::PHYsecTelemetryType::PHYsecTelemetryKeyGenConf => {
                                if role.is_none() {
                                    // awful hack but i did not see how to do otherwise without
                                    // changing the code too much... don't blame the dev, blame the
                                    // tools
                                    unsafe {
                                        let payload_addr = &raw const packet.payload;
                                        let payload_casted =
                                            payload_addr as *const Box<KeyGenConfigPacket>;
                                        // sorry...
                                        role = Some(if (*payload_casted).is_master {
                                            KeygenRole::Alice
                                        } else {
                                            KeygenRole::Bob
                                        })
                                    }
                                    if let Some(role) = &role {
                                        // first we flush current log file
                                        let mut logfile_ref = logfile.as_ref().unwrap();
                                        let mut new_log_file_name =
                                            format!("log_{}_{}.txt", role, timestamp_start);
                                        // then we create new log file
                                        let mut new_log_file = OpenOptions::new()
                                            .write(true)
                                            .create(true)
                                            .open(&new_log_file_name)
                                            .unwrap();
                                        logfile_ref.rewind().unwrap();
                                        let mut logfile_content = Vec::<u8>::new();
                                        let logfile_content_size =
                                            logfile_ref.read_to_end(&mut logfile_content).unwrap();
                                        if logfile_content_size > 0 {
                                            new_log_file.write_all(&logfile_content).unwrap();
                                        }
                                        logfile = Some(new_log_file);
                                        fs::remove_file(&logfile_name);
                                        logfile_name = new_log_file_name;
                                    }
                                }
                            }
                            _ => {}
                        }
                        let mut logmsg = payload.to_log();
                        logmsg.push('\n');
                        logfile
                            .as_mut()
                            .unwrap()
                            .write_all(logmsg.as_bytes())
                            .unwrap();
                    }

                    cur_size = remaining.len();
                    if (cur_size > 0) {
                        // update buffer to keep remaining data
                        rx_buf[..cur_size].copy_from_slice(remaining);
                        //println!("Rem = {:02x?}", &rx_buf[..cur_size]);
                    }
                }
            }
        }
    }

    plog!("Good by(t)e ! 😄");
}
