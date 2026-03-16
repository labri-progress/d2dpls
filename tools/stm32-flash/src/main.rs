use clap::{Parser};
use console::style;
use dialoguer::FuzzySelect;
use indicatif::{ProgressBar, ProgressStyle};
use probe_rs::{
    flashing::{DownloadOptions, FlashProgress},
    probe::list::Lister,
    Permissions, probe::DebugProbeInfo
};
use probe_rs::gdb_server;
use std::sync::{Arc};
use lock_api::{ Mutex };

const STLINK_VID: u16 = 0x0483;
const STLINK_PIDS: [u16; 8] = [
    0x347b, 0x3744, 0x3748, 0x374b, 0x3752, 0x374e, 0x374f, 0x3753,
];

macro_rules! perr {
    ($($arg:tt)*) => {
        eprintln!("{} {}", style("[!]").red().bold(), format!($($arg)*))
    };
}

macro_rules! pinfo {
    ($($arg:tt)*) => {
        println!("{} {}", style("[+]").blue().bold(), format!($($arg)*))
    };
}

macro_rules! plog {
    ($($arg:tt)*) => {
        println!("{} {}", style("[*]").magenta().bold(), format!($($arg)*))
    };
}

/// Command-line arguments parser
#[derive(Parser)]
#[command(author, version, about)]
struct Args {
    /// Path to the ELF file to flash
    #[arg(short, long)]
    elf: Option<String>,

    /// Start the GDB server after flashing
    #[arg(long)]
    gdb: bool,
}

fn main() {
    let args = Args::parse();

    let lister = Lister::new();
    let probes = lister.list_all();

    if probes.is_empty() {
        perr!("No probes detected");
        return;
    }

    pinfo!("Connected probes:");
    let mut stlinks = Vec::new();
    for (index, probe) in probes.iter().enumerate() {
        println!("[{}] {}", index, probe);
        if probe.vendor_id == STLINK_VID && STLINK_PIDS.contains(&probe.product_id) {
            stlinks.push(probe.clone());
        }
    }

    let stm32 = match stlinks.len() {
        0 => {
            perr!("No STLINK found (verify the STM32 board is connected)");
            return;
        }
        l if l >= 2 => {
            let choices = stlinks.iter().map(|p| format!("{}", p)).collect::<Vec<_>>();
            let selection = FuzzySelect::with_theme(&dialoguer::theme::ColorfulTheme::default())
                .with_prompt("[-] Select ST-LINK")
                .default(0)
                .items(&choices[..])
                .interact()
                .unwrap();
            stlinks[selection].clone()
        }
        _ => {
            let stlink = stlinks[0].clone();
            pinfo!("Using ST-LINK: {}", stlink);
            stlink
        }
    };

    // Flash firmware if an ELF file is provided
    if let Some(ref filename) = args.elf {
        flash_firmware(stm32.clone(), filename);
    }

    // Start GDB server if the flag is set
    if args.gdb {
        start_gdb_server(stm32);
    } else if args.elf.is_none() {
        perr!("No action specified. Use --elf <file> to flash and/or --gdb to start GDB server.");
    }
}

fn start_gdb_server(stm32: DebugProbeInfo) {
    plog!("Starting GDB server...");
    let probe = match stm32.open() {
        Ok(p) => p,
        Err(e) => {
            perr!("Error opening probe: {}", e);
            return;
        }
    };

    let session = match probe.attach_under_reset("STM32L072CZYx", Permissions::default()) {
        Ok(s) => s,
        Err(e) => {
            perr!("Error attaching to target: {}", e);
            return;
        }
    };
    
    let gdb_conf = gdb_server::GdbInstanceConfiguration::from_session(&session, None::<String>);
    //let confs = vec![&gdb_conf];

    let lock = Mutex::new(session);

    pinfo!("GDB server started");
    gdb_server::run(&lock, gdb_conf.iter()) 
        .unwrap_or_else(|e| perr!("GDB server error: {}", e));
}

fn flash_firmware(stm32: DebugProbeInfo, filename: &str) {
    let mut file = match std::fs::File::open(&filename) {
        Ok(f) => f,
        Err(e) => {
            perr!("Error opening ELF file ({}): {}", filename, e);
            return;
        }
    };

    let probe = match stm32.open() {
        Ok(p) => p,
        Err(e) => {
            perr!("Error opening probe: {}", e);
            return;
        }
    };

    let mut session = match probe.attach_under_reset("STM32L072CZYx", Permissions::default()) {
        Ok(s) => s,
        Err(e) => {
            perr!("Error attaching to target: {}", e);
            return;
        }
    };

    let mut flash_loader = session.target().flash_loader();

    plog!("Preparing flash operation");

    match flash_loader.load_elf_data(&mut file) {
        Ok(_) => (),
        Err(e) => {
            perr!("Error loading ELF data: {}", e);
            return;
        }
    }

    let data: Vec<(u64, &[u8])> = flash_loader.data().collect();
    let mut num_chunks: u64 = 0;
    for c in data {
        let (_, s) = c;
        num_chunks += (s.len() as u64 / 128) * 2;
    }

    let progress = Arc::new(std::sync::Mutex::new(
        ProgressBar::new(num_chunks).with_style(
            ProgressStyle::default_bar()
                .template(
                    "{spinner:.green} [{elapsed_precise}] [{bar:40.cyan/blue}] {pos}/{len} \
                    ({percent}%) {msg}",
                )
                .unwrap()
                .progress_chars("#>-")
        )
    ));

    let mut dl_opt = DownloadOptions::default();
    dl_opt.verify = true;
    dl_opt.progress = Some(FlashProgress::new(move |event| {
        let progress = progress.clone();
        let progress = progress.lock().unwrap();
        match event {
            probe_rs::flashing::ProgressEvent::Initialized { .. } => {
                progress.set_message("Initialized");
            }
            probe_rs::flashing::ProgressEvent::StartedFilling => {
                progress.set_message("Filling");
            }
            probe_rs::flashing::ProgressEvent::StartedErasing => {
                progress.set_message("Erasing");
            }
            probe_rs::flashing::ProgressEvent::SectorErased { .. } => {
                progress.inc(1);
            }
            probe_rs::flashing::ProgressEvent::FinishedErasing => {
                progress.set_message("Finished Erasing");
            }
            probe_rs::flashing::ProgressEvent::StartedProgramming { .. } => {
                progress.set_message("Programming");
            }
            probe_rs::flashing::ProgressEvent::PageProgrammed { .. } => {
                progress.inc(8);
            }
            probe_rs::flashing::ProgressEvent::FinishedProgramming => {
                progress.set_message("Finished Programming");
                progress.finish();
            }
            _ => {}
        }
    }));

    plog!("Flashing target...");
    match flash_loader.commit(&mut session, dl_opt) {
        Ok(_) => pinfo!("Flashing complete"),
        Err(e) => {
            perr!("Error flashing target: {}", e);
        }
    }
}

