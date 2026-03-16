//! everything related to parsing log files in order to extract metadatas
use std::fs::File;
use std::io::Read;

use chrono::NaiveDateTime;
use regex::Regex;
use thiserror::Error;

use crate::bindings::physec::{
    csi_type_t, preprocess_type_t, quant_type_t, recon_type_t, RetrievableEnum,
};
use crate::key_generations_metadatas::{
    HostTimings, Key, KeyGeneration, KeyGenerations, KeyGenerationsConf, KeyGenerationsMetadata,
    KeyPair, ReceivedBits, SingleParticipantKeyGeneration, SingleParticipantKeyGenerations,
    SingleParticipantKeyGenerationsMetadata,
};

/// we want to discern inconsistencies in log files. ideally we don't want to leave data thas is
/// incoherent invisible, but we still need to be flexible enough so we don't throw a whole
/// experiment for something that wouldn't impact the final data
#[derive(Error, Debug)]
pub enum ExtractorError {
    #[error("Regex Error: {0}")]
    RegexError(String),
    #[error("IO Error: {0}")]
    IoError(String),
    #[error("Date Parsing Error: {0}")]
    DateParsingError(String),
    #[error("Hex Decoding Error: {0}")]
    HexDecodingError(String),
    #[error("Data Inconsistency: {0}")]
    DataInconsistency(String),
    #[error("Data Not Found: {0}")]
    DataNotFound(String),
    #[error("Parse Error: {0}")]
    ParseError(String),
    #[error("Timing Extraction Error: {0}")]
    TimingExtractionError(String),
    #[error("Configuration Error: {0}")]
    ConfigError(String),
    #[error("Key Length Error: {0}")]
    KeyLengthError(String),
    #[error("Experiment Timings Error: {0}")]
    ExperimentTimingsError(String),
    #[error("Received bits extraction error: {0}")]
    ReceivedBitsExtractionError(String),
    #[error("Got two identical roles in log files, both are {0}")]
    IdenticalLogFilesRoles(String),
}

#[derive(Debug)]
pub struct SingleParticipantRunLines<'a> {
    quantized_key: Vec<&'a str>,
    processed_key: Option<Vec<&'a str>>,
    recon_key: Option<Vec<&'a str>>,
    start_keygen: Vec<&'a str>,
    end_keygen: Vec<&'a str>,
    received_bits: Vec<Vec<&'a str>>,
}

impl<'a> SingleParticipantRunLines<'a> {
    pub const fn new(
        quantized_key: Vec<&'a str>,
        processed_key: Option<Vec<&'a str>>,
        recon_key: Option<Vec<&'a str>>,
        start_keygen: Vec<&'a str>,
        end_keygen: Vec<&'a str>,
        received_bits: Vec<Vec<&'a str>>,
    ) -> Self {
        Self {
            quantized_key,
            processed_key,
            recon_key,
            start_keygen,
            end_keygen,
            received_bits,
        }
    }
}

/// each vector in this struct is the result of a regex capture. every member of said vectors are a
/// capture group result.
#[derive(Debug)]
pub struct AliceAndBobRunLines<'a> {
    alice: SingleParticipantRunLines<'a>,
    bob: SingleParticipantRunLines<'a>,
}

impl<'a> AliceAndBobRunLines<'a> {
    pub const fn new(
        alice: SingleParticipantRunLines<'a>,
        bob: SingleParticipantRunLines<'a>,
    ) -> Self {
        Self { alice, bob }
    }
}

/// does the dirty work of analyzing the raw file buffers and extracting the metadatas
/// does most of that using regex certified bio from any llm (and as optimal as i could get them...)
/// TODO: enhancing the regex use, right now we deal with kind of "anonymous" vectors and that's not
/// cool. ideally i would like to implement some wrapper for every regexs that would abstract them
/// and offer named capture groups instead of optional vector elements.
pub struct Extractor {
    pub key_length: usize,
    keygen_conf_regex: Regex,
    start_keygen_regex: Regex,
    end_keygen_regex: Regex,
    quantized_key_regex: Regex,
    processed_key_regex: Regex,
    reconciliated_key_regex: Regex,
    key_is_ready: Regex,
    received_bits_regex: Regex,
}

impl Extractor {
    const HOST_TIMESTAMP_REGEX: &'static str = r"(\d{4}(?:-\d{2}){2}_(?:\d{2}-){3}.\d{3})";
    const DEVICE_TIMESTAMP_REGEX: &'static str = r"(\d+.\d{1,3})";
    const KEY_REGEX: &'static str = r"\[((?:[0-9a-f]{2}(?:, )?)+)\]";
    const DEFAULT_KEY_LENGTH: usize = 128;

    /// creates an extractor with default key length
    pub fn new() -> Result<Self, ExtractorError> {
        Self::with_key_length(Self::DEFAULT_KEY_LENGTH)
    }

    /// creates an extractor from specified key length. we use const key length atm but in the
    /// future there could be multiple sizes or experiment-specific sizes
    pub fn with_key_length(key_length: usize) -> Result<Self, ExtractorError> {
        if key_length > 1 << 23 {
            return Err(ExtractorError::KeyLengthError(
                "KEY_LENGTH exceeds f32 precision limit".to_string(),
            ));
        }

        let keygen_conf_regex = Regex::new(
            r"(?m)^Master: (true|false), CSI Type: (\d+), Pre-process Type: (\d+), Quant Type: (\d+), Recon Type: (\d+), Probe Delay: (\d+)$"
        ).map_err(|e| ExtractorError::RegexError(format!("Failed to compile keygen_conf regex: {e}")))?;

        let start_keygen_regex = Regex::new(&format!(
            r"(?m)^{} {} (?:(?:[<|>] Probe Sent ! \(cnt=0\)\n{} {} < received)|(?:> Keygen Done ! \(\d+ bits\)))",
            Self::HOST_TIMESTAMP_REGEX,
            Self::DEVICE_TIMESTAMP_REGEX,
            Self::HOST_TIMESTAMP_REGEX,
            Self::DEVICE_TIMESTAMP_REGEX,

        ))
        .map_err(|e| {
            ExtractorError::RegexError(format!("Failed to compile keygen_conf regex: {e}"))
        })?;

        let end_keygen_regex = Regex::new(&format!(
            r"(?m)^{} {} Performing Reset !$",
            Self::HOST_TIMESTAMP_REGEX,
            Self::DEVICE_TIMESTAMP_REGEX
        ))
        .map_err(|e| {
            ExtractorError::RegexError(format!("Failed to compile end_keygen regex: {e}"))
        })?;

        let quantized_key_regex = Regex::new(&format!(r"(?m)^Quant Key = {}$", Self::KEY_REGEX))
            .map_err(|e| {
                ExtractorError::RegexError(format!("Failed to compile quantized_key regex: {e}"))
            })?;

        let processed_key_regex = Regex::new(&format!(
            r"(?m)^Post-processing Key = {}$",
            Self::KEY_REGEX
        ))
        .map_err(|e| {
            ExtractorError::RegexError(format!("Failed to compile processed_key regex: {e}"))
        })?;

        let reconciliated_key_regex = Regex::new(&format!(
            r"(?m)^Reconciliation Key = {}$",
            Self::KEY_REGEX
        ))
        .map_err(|e| {
            ExtractorError::RegexError(format!("Failed to compile reconciliated_key regex: {e}"))
        })?;

        let key_is_ready = Regex::new(&format!(
            r"(?m)^{} {} Key Ready !$",
            Self::HOST_TIMESTAMP_REGEX,
            Self::DEVICE_TIMESTAMP_REGEX,
        ))
        .map_err(|e| {
            ExtractorError::RegexError(format!("Failed to compile reconciliated_key regex: {e}"))
        })?;

        let received_bits_regex = Regex::new(&format!(
            r"(?m)^{} {} < received (\d+) bytes.$",
            Self::HOST_TIMESTAMP_REGEX,
            Self::DEVICE_TIMESTAMP_REGEX,
        ))
        .map_err(|e| {
            ExtractorError::RegexError(format!("Failed to compile reconciliated_key regex: {e}"))
        })?;

        Ok(Self {
            key_length,
            keygen_conf_regex,
            start_keygen_regex,
            end_keygen_regex,
            quantized_key_regex,
            processed_key_regex,
            reconciliated_key_regex,
            key_is_ready,
            received_bits_regex,
        })
    }

    pub fn extract_metadatas_single(
        &self,
        file_path: &str,
    ) -> Result<SingleParticipantKeyGenerationsMetadata, ExtractorError> {
        let file_buffer = Self::read_log_file(file_path)?;
        let conf = Self::extract_matches(&self.keygen_conf_regex, &file_buffer);
        let conf = conf
            .first()
            .ok_or_else(|| ExtractorError::ConfigError("couldn't extract file conf".to_string()))?;
        let conf = Self::extract_conf(conf)?;
        let (start_time, end_time, full_duration) =
            self.extract_experiment_timings(&file_buffer)?;
        let mut keygens: Vec<SingleParticipantKeyGeneration> = vec![];
        let extracted_run_lines = self.extract_single_log_keygen(&file_buffer)?;

        for extracted_run_line in extracted_run_lines {
            if let Some(quant_key_str) = extracted_run_line.quantized_key.first() {
                let quant_key = Self::str_to_key(self.key_length, quant_key_str)?;
                let mut proc_key = None;
                let mut recon_key = None;
                if let Some(_proc_key) = extracted_run_line.processed_key {
                    if let Some(_proc_key) = _proc_key.first() {
                        proc_key = Some(Key::new(&Self::str_to_key(self.key_length, _proc_key)?));
                    }
                }
                if let Some(_rec_key) = extracted_run_line.recon_key {
                    if let Some(_rec_key) = _rec_key.first() {
                        recon_key = Some(Key::new(&Self::str_to_key(self.key_length, _rec_key)?));
                    }
                }
                let received_bits = Self::received_bits(&extracted_run_line.received_bits)?;
                let (host_start, host_end, duration) = Self::extract_timings(
                    &extracted_run_line.start_keygen,
                    &extracted_run_line.end_keygen,
                )?;
                let host_timings = HostTimings::new(host_start, host_end);
                let kg = SingleParticipantKeyGeneration::new(
                    proc_key,
                    recon_key,
                    Key::new(&quant_key),
                    host_timings,
                    duration,
                    received_bits,
                );
                keygens.push(kg);
            } else {
                todo!()
            }
        }
        Ok(SingleParticipantKeyGenerationsMetadata::new(
            conf,
            SingleParticipantKeyGenerations(keygens),
            start_time,
            end_time,
            full_duration,
        ))
    }
    /// extracts the metadatas from a couple of log files
    pub fn extract_metadatas(
        &self,
        first_file_path: &str,
        second_file_path: &str,
    ) -> Result<KeyGenerationsMetadata, ExtractorError> {
        let (first_file_buffer, second_file_buffer) =
            Self::read_log_files(first_file_path, second_file_path)?;
        let first_file_conf = Self::extract_matches(&self.keygen_conf_regex, &first_file_buffer);
        let second_file_conf = Self::extract_matches(&self.keygen_conf_regex, &second_file_buffer);
        let confs = Self::extract_confs(
            first_file_conf.first().ok_or_else(|| {
                ExtractorError::ConfigError("couldn't extract first file conf".to_string())
            })?,
            second_file_conf.first().ok_or_else(|| {
                ExtractorError::ConfigError("couldn't extract second file conf".to_string())
            })?,
        )?;

        let alice_logs_buffer: String;
        let bob_logs_buffer: String;
        match (confs.0.is_master, confs.1.is_master) {
            (true, false) => {
                alice_logs_buffer = first_file_buffer;
                bob_logs_buffer = second_file_buffer;
            }
            (false, true) => {
                alice_logs_buffer = second_file_buffer;
                bob_logs_buffer = first_file_buffer;
            }
            (true, true) => {
                return Err(ExtractorError::IdenticalLogFilesRoles("alice".to_string()))
            }
            (false, false) => {
                return Err(ExtractorError::IdenticalLogFilesRoles("bob".to_string()))
            }
        }

        let (start_time, end_time, full_duration) =
            self.extract_experiment_timings(&alice_logs_buffer)?;
        let extracted_run_lines =
            self.extract_alice_bob_keygens(&alice_logs_buffer, &bob_logs_buffer)?;
        let mut keygens: Vec<KeyGeneration> = vec![];
        for extracted_run_line in extracted_run_lines {
            keygens.push(self.process_key_generation(&extracted_run_line)?);
        }
        Ok(KeyGenerationsMetadata::new(
            confs,
            KeyGenerations(keygens),
            start_time,
            end_time,
            full_duration,
        ))
    }

    pub fn extract_individual_keygen<'a>(
        &'a self,
        log_buffer: &'a str,
        current_pos: usize,
        participant_name: &str,
    ) -> Result<(SingleParticipantRunLines<'a>, usize), ExtractorError> {
        let first_keygen_start = self.start_keygen_regex.find_at(log_buffer, current_pos);
        let first_keygen_end = self.end_keygen_regex.find_at(log_buffer, current_pos);

        if let (Some(first_keygen_start), Some(first_keygen_end)) =
            (first_keygen_start, first_keygen_end)
        {
            let keygen_buffer = &log_buffer[first_keygen_start.start()..first_keygen_end.end()];
            let start_keygen_matches =
                Self::extract_matches(&self.start_keygen_regex, keygen_buffer);
            let end_keygen_matches = Self::extract_matches(&self.end_keygen_regex, keygen_buffer);
            let quantized_key_matches =
                Self::extract_matches(&self.quantized_key_regex, keygen_buffer);
            let processed_key_matches =
                Self::extract_matches(&self.processed_key_regex, keygen_buffer);
            let reconciliated_key_matches =
                Self::extract_matches(&self.reconciliated_key_regex, keygen_buffer);
            let received_bits = self.extract_received_bits(keygen_buffer);

            let quant_key = quantized_key_matches
                .first()
                .ok_or_else(|| {
                    ExtractorError::DataNotFound(format!("quantized_key_{participant_name}"))
                })?
                .clone();
            let proc_key = processed_key_matches.first().cloned();
            let recon_key = reconciliated_key_matches.first().cloned();
            let start_kg = start_keygen_matches
                .first()
                .ok_or_else(|| {
                    ExtractorError::DataNotFound(format!("start_kg_{participant_name}"))
                })?
                .clone();
            let end_kg = end_keygen_matches
                .first()
                .ok_or_else(|| ExtractorError::DataNotFound(format!("end_kg_{participant_name}")))?
                .clone();

            let participant = SingleParticipantRunLines::new(
                quant_key,
                proc_key,
                recon_key,
                start_kg,
                end_kg,
                received_bits,
            );

            Ok((participant, first_keygen_end.end()))
        } else {
            Err(ExtractorError::DataNotFound(format!(
                "keygen start or end for {participant_name}"
            )))
        }
    }

    // because later it could be cool to handle better... TODO:
    #[allow(clippy::unnecessary_wraps)]
    pub fn extract_single_log_keygen<'a>(
        &'a self,
        log_buffer: &'a str,
    ) -> Result<Vec<SingleParticipantRunLines<'a>>, ExtractorError> {
        let mut cur = 0usize;
        let mut keygen_run_lines: Vec<SingleParticipantRunLines> = vec![];
        let nb_keygens = self.end_keygen_regex.captures_iter(log_buffer).count();

        for _ in 0..nb_keygens {
            let result = self.extract_individual_keygen(log_buffer, cur, "log file");
            if let Ok((data, new_cur)) = result {
                keygen_run_lines.push(data);
                cur = new_cur;
            } else {
                println!("todo: better handle this");
            }
        }
        Ok(keygen_run_lines)
    }

    pub fn extract_alice_bob_keygens<'a>(
        &'a self,
        alice_logs_buffer: &'a str,
        bob_logs_buffer: &'a str,
    ) -> Result<Vec<AliceAndBobRunLines<'a>>, ExtractorError> {
        let mut cur_alice = 0usize;
        let mut cur_bob = 0usize;
        let mut keygens_run_lines: Vec<AliceAndBobRunLines> = vec![];
        let nb_keygens_a = self
            .end_keygen_regex
            .captures_iter(alice_logs_buffer)
            .count();
        let nb_keygens_b = self.end_keygen_regex.captures_iter(bob_logs_buffer).count();
        if nb_keygens_a != nb_keygens_b {
            return Err(ExtractorError::DataInconsistency(format!(
                "not same amount of keygens in A ({nb_keygens_a}) and B ({nb_keygens_b})",
            )));
        }
        for exp_idx in 0..nb_keygens_a {
            let alice_result =
                self.extract_individual_keygen(alice_logs_buffer, cur_alice, "alice");
            let bob_result = self.extract_individual_keygen(bob_logs_buffer, cur_bob, "bob");
            match (alice_result, bob_result) {
                (Ok((alice, new_cur_alice)), Ok((bob, new_cur_bob))) => {
                    let extracted_run_line = AliceAndBobRunLines::new(alice, bob);
                    keygens_run_lines.push(extracted_run_line);
                    cur_alice = new_cur_alice;
                    cur_bob = new_cur_bob;
                }
                _ => eprintln!("error extracting experience {}", exp_idx + 1),
            }
        }
        Ok(keygens_run_lines)
    }

    /// returns the timings of the whole experiments, so all the key generations. for single key
    /// timing extraction see below.
    /// (Start Time (host), End Time (host), Duration (device))
    fn extract_experiment_timings(
        &self,
        full_log: &str,
    ) -> Result<(NaiveDateTime, NaiveDateTime, f32), ExtractorError> {
        let start_keygens = Self::extract_matches(&self.start_keygen_regex, full_log);
        let start_keygens = start_keygens.first().ok_or_else(|| {
            ExtractorError::ExperimentTimingsError(
                "Couldn't find experimentations start".to_string(),
            )
        })?;
        let end_keygens = Self::extract_matches(&self.end_keygen_regex, full_log);
        let end_keygens = end_keygens.last().ok_or_else(|| {
            ExtractorError::ExperimentTimingsError("Couldn't find experimentations end".to_string())
        })?;

        if let (Some(start_time), Some(end_time)) = (start_keygens.first(), end_keygens.first()) {
            let start_keygens = Self::parse_device_time(start_time)?;
            let end_keygens = Self::parse_device_time(end_time)?;
            let keygens_duration = end_keygens - start_keygens;
            Ok((
                start_keygens,
                end_keygens,
                keygens_duration.as_seconds_f32(),
            ))
        } else {
            Err(ExtractorError::ExperimentTimingsError(
                "Couldn't extract experiment timings".to_string(),
            ))
        }
    }

    fn read_log_file(file_path: &str) -> Result<String, ExtractorError> {
        let mut file = File::open(file_path).map_err(|e| {
            ExtractorError::IoError(format!("couldn't open logs file {file_path}: {e}"))
        })?;
        let mut contents = String::new();
        file.read_to_string(&mut contents)
            .map_err(|e| ExtractorError::IoError(format!("couldn't read logs: {e}")))?;
        Ok(contents)
    }

    fn read_log_files(
        first_file_path: &str,
        second_file_path: &str,
    ) -> Result<(String, String), ExtractorError> {
        let alice_logs = Self::read_log_file(first_file_path)?;
        let bob_logs = Self::read_log_file(second_file_path)?;
        Ok((alice_logs, bob_logs))
    }

    /// extracts a vector of matches from a regex. each inner-vector is a match, their content being the capture groups.
    /// that's what I said we could replace by some proper datatypes and custom logic :)
    fn extract_matches<'a>(regex: &Regex, text: &'a str) -> Vec<Vec<&'a str>> {
        let res: Vec<Vec<&str>> = regex
            .captures_iter(text)
            .map(|captures| {
                captures
                    .iter()
                    .skip(1) // skip the first capture group (whole regex match)
                    .map(|m| m.map_or("", |match_| match_.as_str()))
                    .collect()
            })
            .collect();
        res
    }

    /// extracts the config, replacing raw numbers by what they actually represent (extracted from
    /// bindgen script)
    fn extract_conf(conf: &[&str]) -> Result<KeyGenerationsConf, ExtractorError> {
        fn parse_error(field: &str) -> ExtractorError {
            ExtractorError::ParseError(format!("could not parse {field} field"))
        }
        fn config_error(field: &str) -> ExtractorError {
            ExtractorError::ConfigError(format!("missing {field} field"))
        }

        let is_master = *(conf.first().ok_or_else(|| config_error("is_master"))?) == "true";
        let csi_type = csi_type_t::retrieve(
            conf.get(1)
                .ok_or_else(|| config_error("csi_type"))?
                .parse::<usize>()
                .map_err(|_| parse_error("csi_type"))?,
        )
        .map_err(|_| parse_error("csi_type"))?;
        let pre_process_type = preprocess_type_t::retrieve(
            conf.get(2)
                .ok_or_else(|| config_error("pre_process_type"))?
                .parse::<usize>()
                .map_err(|_| parse_error("pre_process_type"))?,
        )
        .map_err(|_| parse_error("pre_process_type"))?;
        let quant_type = quant_type_t::retrieve(
            conf.get(3)
                .ok_or_else(|| config_error("quant_type"))?
                .parse::<usize>()
                .map_err(|_| parse_error("quant_type"))?,
        )
        .map_err(|_| parse_error("quant_type"))?;
        let recon_type = recon_type_t::retrieve(
            conf.get(4)
                .ok_or_else(|| config_error("recon_type"))?
                .parse::<usize>()
                .map_err(|_| parse_error("recon_type"))?,
        )
        .map_err(|_| parse_error("recon_type"))?;
        let probe_delay = conf
            .get(5)
            .ok_or_else(|| config_error("probe_delay"))?
            .parse::<usize>()
            .map_err(|_| parse_error("probe_delay"))?;

        Ok(KeyGenerationsConf::new(
            is_master,
            csi_type,
            pre_process_type,
            quant_type,
            recon_type,
            probe_delay,
        ))
    }

    fn extract_confs(
        first_file_match: &[&str],
        second_file_match: &[&str],
    ) -> Result<(KeyGenerationsConf, KeyGenerationsConf), ExtractorError> {
        Ok((
            Self::extract_conf(first_file_match)?,
            Self::extract_conf(second_file_match)?,
        ))
    }

    /// extracts the timings of a single experiment, same as the one for whole experiments timings
    fn extract_timings<'a>(
        start_keygen: &'a Vec<&'a str>,
        end_keygen: &'a Vec<&'a str>,
    ) -> Result<(NaiveDateTime, NaiveDateTime, f32), ExtractorError> {
        if let (
            Some(start_time_device_str),
            Some(end_time_device_str),
            Some(start_time_host_str),
            Some(end_time_host_str),
        ) = (
            start_keygen.get(1),
            end_keygen.get(1),
            start_keygen.first(),
            end_keygen.first(),
        ) {
            if let (Ok(start_time_device), Ok(end_time_device)) = (
                start_time_device_str.parse::<f32>(),
                end_time_device_str.parse::<f32>(),
            ) {
                let start_time = Self::parse_device_time(start_time_host_str)?;
                let end_time = Self::parse_device_time(end_time_host_str)?;
                Ok((start_time, end_time, end_time_device - start_time_device))
            } else {
                Err(ExtractorError::ParseError(
                    "failed parsing start or end time".to_string(),
                ))
            }
        } else {
            Err(ExtractorError::TimingExtractionError(
                "missing first or last keygen device time info".to_string(),
            ))
        }
    }

    /// converts a raw key match from the regex to a vector of bytes.
    pub(crate) fn str_to_key(key_length: usize, key_str: &str) -> Result<Vec<u8>, ExtractorError> {
        let raw_key_str = key_str.replace(", ", "");
        if raw_key_str.len() < key_length / 4 {
            return Err(ExtractorError::KeyLengthError(format!(
                "key is not long enough. Expected at least {}, got {}",
                key_length / 4,
                raw_key_str.len()
            )));
        }
        let key_str = &raw_key_str[..key_length / 4];
        let hex_key = hex::decode(key_str).map_err(|e| {
            ExtractorError::HexDecodingError(format!("couldn't decode hex key: {e}"))
        })?;
        if hex_key.len() < key_length / 8 {
            return Err(ExtractorError::KeyLengthError(format!(
                "decoded key is too short. Expected at least {}, got {}",
                key_length / 8,
                hex_key.len()
            )));
        }
        Ok(hex_key[..key_length / 8].to_vec())
    }

    fn received_bits(received_bits_matches: &Vec<Vec<&str>>) -> Result<usize, ExtractorError> {
        let mut received_bits = 0;
        for received_bits_a_raw in received_bits_matches {
            if let Some(bits_str) = received_bits_a_raw.get(2) {
                let bits = bits_str
                    .parse::<usize>()
                    .map_err(|err| ExtractorError::ReceivedBitsExtractionError(err.to_string()))?;
                received_bits += bits;
            }
        }
        Ok(received_bits)
    }

    fn process_key_generation(
        &self,
        extracted_run: &AliceAndBobRunLines,
    ) -> Result<KeyGeneration, ExtractorError> {
        let (host_start_time, host_end_time, duration_s) = Self::extract_timings(
            &extracted_run.alice.start_keygen,
            &extracted_run.alice.end_keygen,
        )?;
        let kgr = KeyGeneration::compute_kgr(self.key_length, duration_s);

        if let (Some(alice_quantized_key_str), Some(bob_quantized_key_str)) = (
            extracted_run.alice.quantized_key.first(),
            extracted_run.bob.quantized_key.first(),
        ) {
            let alice_quantized_key = Self::str_to_key(self.key_length, alice_quantized_key_str)?;
            let bob_quantized_key = Self::str_to_key(self.key_length, bob_quantized_key_str)?;
            let post_quantization_keypair =
                KeyPair::new(Key::new(&alice_quantized_key), Key::new(&bob_quantized_key));
            let mut post_processing_keypair = None;
            if let (Some(processed_key_alice), Some(processed_key_bob)) = (
                &extracted_run.alice.processed_key,
                &extracted_run.bob.processed_key,
            ) {
                if let (Some(processed_key_alice), Some(processed_key_bob)) =
                    (processed_key_alice.first(), processed_key_bob.first())
                {
                    let processed_key_alice =
                        Self::str_to_key(self.key_length, processed_key_alice)?;
                    let processed_key_bob = Self::str_to_key(self.key_length, processed_key_bob)?;
                    post_processing_keypair = Some(KeyPair::new(
                        Key::new(&processed_key_alice),
                        Key::new(&processed_key_bob),
                    ));
                }
            }
            let mut post_recon_keypair = None;
            if let (Some(recon_key_alice), Some(recon_key_bob)) =
                (&extracted_run.alice.recon_key, &extracted_run.bob.recon_key)
            {
                if let (Some(recon_key_alice), Some(recon_key_bob)) =
                    (recon_key_alice.first(), recon_key_bob.first())
                {
                    let recon_key_alice = Self::str_to_key(self.key_length, recon_key_alice)?;
                    let recon_key_bob = Self::str_to_key(self.key_length, recon_key_bob)?;
                    post_recon_keypair = Some(KeyPair::new(
                        Key::new(&recon_key_alice),
                        Key::new(&recon_key_bob),
                    ));
                }
            }

            let received_bits_a = Self::received_bits(&extracted_run.alice.received_bits)?;
            let received_bits_b = Self::received_bits(&extracted_run.bob.received_bits)?;
            let received_bits = ReceivedBits::new(received_bits_a, received_bits_b);
            let host_timings = HostTimings::new(host_start_time, host_end_time);
            let key_generation = KeyGeneration::new(
                kgr,
                post_processing_keypair,
                post_quantization_keypair,
                post_recon_keypair,
                host_timings,
                duration_s,
                received_bits,
            );
            return Ok(key_generation);
        }
        Err(ExtractorError::DataNotFound(
            "missing required key data".to_string(),
        ))
    }

    fn extract_received_bits<'a>(&self, logs_buffer: &'a str) -> Vec<Vec<&'a str>> {
        Self::extract_matches(&self.received_bits_regex, logs_buffer)
    }

    fn parse_device_time(datetime_str: &str) -> Result<NaiveDateTime, ExtractorError> {
        NaiveDateTime::parse_from_str(datetime_str, "%F_%H-%M-%S-%.3f")
            .map_err(|e| ExtractorError::DateParsingError(format!("({datetime_str}): {e}")))
    }
}

impl Default for Extractor {
    fn default() -> Self {
        Self::new().expect("Failed to create default Extractor")
    }
}
