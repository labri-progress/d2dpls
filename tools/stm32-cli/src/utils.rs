pub(crate) use console::style;

macro_rules! perr {
    ($($arg:tt)*) => {
        eprintln!("{} {}", style("[!]").red().bold(), format!($($arg)*))
    };
}

macro_rules! pinfo {
    ($($arg:tt)*) => {
        println!("{} {}", style("[+]").green().bold(), format!($($arg)*))
    };
}

macro_rules! plog {
    ($($arg:tt)*) => {
        println!("{} {}", style("[*]").magenta().bold(), format!($($arg)*))
    };
}

macro_rules! plist {
    ($($arg:tt)*) => {
        println!("{} {}", style("[-]").blue().bold(), format!($($arg)*))
    };
}

pub(crate) use perr;
pub(crate) use pinfo;
pub(crate) use plist;
pub(crate) use plog;
