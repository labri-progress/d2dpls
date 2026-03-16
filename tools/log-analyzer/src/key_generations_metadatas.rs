//! datatypes that handles the final key generations metadatas informations.
//!
//! at this point, expect no heavy analysis, just some computations from already extracted data
use std::{
    fmt::{self, Display},
    iter::zip,
};

use chrono::NaiveDateTime;
use colored::Colorize;

use crate::{
    bindings::physec::{csi_type_t, preprocess_type_t, quant_type_t, recon_type_t},
    rng_metadatas::{AliceAndBobTabledRngMetadatas, RngMetadatas},
};

#[derive(Debug)]
pub enum KeyType {
    Processed,
    Quantized,
    Reconciliated,
}

#[derive(Debug)]
pub enum Who {
    Alice,
    Bob,
}

#[derive(Debug)]
pub struct Key {
    key: Vec<u8>,
}

// allowing it because otherwise it doesn't work on older rust versions if applying the fix
#[allow(clippy::missing_const_for_fn)]
impl Key {
    pub fn new(key: &[u8]) -> Self {
        Self { key: key.to_vec() }
    }

    pub fn from_keys<'a>(keys: &'a [&'a [u8]]) -> Self {
        Self { key: keys.concat() }
    }

    pub fn len(&self) -> usize {
        self.key.len() * 8
    }

    pub fn len_bytes(&self) -> usize {
        self.key.len()
    }

    pub fn bytes(&self) -> &[u8] {
        &self.key
    }
}

impl Display for Key {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            self.bytes().iter().fold(String::new(), |acc, byte| {
                if acc.is_empty() {
                    format!("{byte:#X}")
                } else {
                    format!("{acc}, {byte:#X}")
                }
            })
        )
    }
}

#[derive(Debug)]
pub struct KeyPair {
    kar: f32,
    hamming_distance: usize,
    pub keys: (Key, Key),
}

impl KeyPair {
    pub fn new(key_a: Key, key_b: Key) -> Self {
        Self {
            kar: Self::compute_kar(&key_a, &key_b),
            hamming_distance: Self::compute_hamming_distance(&key_a, &key_b),
            keys: (key_a, key_b),
        }
    }

    fn compute_hamming_distance(key_a: &Key, key_b: &Key) -> usize {
        let mut nb_diverging_bits = 0usize;
        for (key_part_a, key_part_b) in zip(key_a.bytes(), key_b.bytes()) {
            let diff = key_part_a ^ key_part_b;
            nb_diverging_bits += diff.count_ones() as usize;
        }
        nb_diverging_bits
    }

    #[allow(clippy::cast_precision_loss)]
    fn compute_kar(key_a: &Key, key_b: &Key) -> f32 {
        let key_length = key_a.len().max(key_b.len()) as f32;
        let nb_diverging_bits = Self::compute_hamming_distance(key_a, key_b) as f32;
        (key_length - nb_diverging_bits) / key_length
    }
}

impl fmt::Display for KeyPair {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let key_0_rngmetadatas = RngMetadatas::new(&self.keys.0);
        let key_1_rngmetadatas = RngMetadatas::new(&self.keys.1);
        write!(
            f,
            "\n\tKey Agreement Rate after quantization: {:.2} - Hamming Distance after quantization: {} bits\n\tA: {}\n\tB: {}",
            self.kar, self.hamming_distance, self.keys.0, self.keys.1
        )?;

        let alice_and_bob =
            AliceAndBobTabledRngMetadatas::new(&key_0_rngmetadatas, &key_1_rngmetadatas, 8);

        write!(f, "\n{alice_and_bob}")
    }
}

#[derive(Debug)]
pub struct SingleParticipantKeyGenerations(pub Vec<SingleParticipantKeyGeneration>);

impl fmt::Display for SingleParticipantKeyGenerations {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            self.0
                .iter()
                .enumerate()
                .map(|(i, m)| format!("{}\n{}", format!("===Key n°{}===", i + 1).bold(), m))
                .collect::<Vec<_>>()
                .join("\n")
        )
    }
}
#[derive(Debug)]
pub struct KeyGenerations(pub Vec<KeyGeneration>);

impl KeyGenerations {
    pub fn aggregate_keys(&self, key_type: KeyType, who: Who) -> Option<Key> {
        match (key_type, who) {
            (KeyType::Processed, Who::Alice) => self
                .0
                .iter()
                .find_map(|kg| kg.post_processing.as_ref())
                .map(|processed| Key::from_keys(&[processed.keys.0.bytes()])),
            (KeyType::Processed, Who::Bob) => self
                .0
                .iter()
                .find_map(|kg| kg.post_processing.as_ref())
                .map(|processed| Key::from_keys(&[processed.keys.1.bytes()])),
            (KeyType::Quantized, Who::Alice) => {
                let keys: Vec<&[u8]> = self
                    .0
                    .iter()
                    .map(|kg| kg.post_quantization.keys.0.bytes())
                    .collect();
                Some(Key::from_keys(&keys))
            }
            (KeyType::Quantized, Who::Bob) => {
                let keys: Vec<&[u8]> = self
                    .0
                    .iter()
                    .map(|kg| kg.post_quantization.keys.1.bytes())
                    .collect();
                Some(Key::from_keys(&keys))
            }
            (KeyType::Reconciliated, Who::Alice) => self
                .0
                .iter()
                .find_map(|kg| kg.post_reconciliation.as_ref())
                .map(|reconciliated| Key::from_keys(&[reconciliated.keys.0.bytes()])),

            (KeyType::Reconciliated, Who::Bob) => self
                .0
                .iter()
                .find_map(|kg| kg.post_reconciliation.as_ref())
                .map(|reconciliated| Key::from_keys(&[reconciliated.keys.1.bytes()])),
        }
    }
}

impl fmt::Display for KeyGenerations {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            self.0
                .iter()
                .enumerate()
                .map(|(i, m)| format!("{}\n{}", format!("===Key n°{}===", i + 1).bold(), m))
                .collect::<Vec<_>>()
                .join("\n")
        )
    }
}

#[derive(Debug)]
pub struct HostTimings {
    start: NaiveDateTime,
    end: NaiveDateTime,
}

impl HostTimings {
    pub const fn new(start: NaiveDateTime, end: NaiveDateTime) -> Self {
        Self { start, end }
    }
}

#[derive(Debug)]
pub struct ReceivedBits {
    alice: usize,
    bob: usize,
}

impl ReceivedBits {
    pub const fn new(alice: usize, bob: usize) -> Self {
        Self { alice, bob }
    }
}

#[derive(Debug)]
pub struct SingleParticipantKeyGeneration {
    post_processed: Option<Key>,
    post_reconciliation: Option<Key>,
    post_quantization: Key,
    host_timings: HostTimings,
    duration_s: f32,
    received_bits: usize,
}

impl SingleParticipantKeyGeneration {
    pub const fn new(
        post_processed: Option<Key>,
        post_reconciliation: Option<Key>,
        post_quantization: Key,
        host_timings: HostTimings,
        duration_s: f32,
        received_bits: usize,
    ) -> Self {
        Self {
            post_processed,
            post_reconciliation,
            post_quantization,
            host_timings,
            duration_s,
            received_bits,
        }
    }
}

impl fmt::Display for SingleParticipantKeyGeneration {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "\tStart Time (host): {}", self.host_timings.start)?;
        writeln!(f, "\tEnd Time (host): {}", self.host_timings.end)?;
        writeln!(f, "\tTotal Elapsed Time: {:.2}s", self.duration_s)?;
        writeln!(
            f,
            "\tTotal Number Of Received Bytes: {} bytes",
            self.received_bits
        )?;
        writeln!(f, "Post Quantization Key:   {}", self.post_quantization)?;
        if let Some(post_processed) = &self.post_processed {
            writeln!(f, "Post Processed Key:      {post_processed}")?;
        }
        if let Some(post_recon) = &self.post_reconciliation {
            writeln!(f, "Post Reconciliation Key: {post_recon}")?;
        }
        let post_quant_metadatas = RngMetadatas::new(&self.post_quantization);
        let mut post_rec_metadatas: Option<&RngMetadatas> = None;
        let mut post_proc_metadatas: Option<&RngMetadatas> = None;
        let rng_metadatas;
        if let Some(k) = &self.post_processed {
            rng_metadatas = RngMetadatas::new(k);
            post_proc_metadatas = Some(&rng_metadatas);
        }
        let rng_metadatas;
        if let Some(k) = &self.post_reconciliation {
            rng_metadatas = RngMetadatas::new(k);
            post_rec_metadatas = Some(&rng_metadatas);
        }

        let metadatas = SingleKeyGenerationRngMetadatas::new(
            &post_quant_metadatas,
            post_proc_metadatas,
            post_rec_metadatas,
            8,
        );

        write!(f, "\nRNG:\n{metadatas}")?;

        Ok(())
    }
}

#[derive(Debug)]
pub struct SingleKeyGenerationRngMetadatas<'a> {
    quant: &'a RngMetadatas<'a>,
    proc: Option<&'a RngMetadatas<'a>>,
    rec: Option<&'a RngMetadatas<'a>>,
    block_size: i32,
}

impl<'a> SingleKeyGenerationRngMetadatas<'a> {
    pub const fn new(
        quant: &'a RngMetadatas,
        proc: Option<&'a RngMetadatas>,
        rec: Option<&'a RngMetadatas>,
        block_size: i32,
    ) -> Self {
        Self {
            quant,
            proc,
            rec,
            block_size,
        }
    }
}

#[allow(clippy::too_many_lines)] // TODO: get rid of this
impl fmt::Display for SingleKeyGenerationRngMetadatas<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut builder = tabled::builder::Builder::default();
        builder.push_record(["Test Name", "Quant Result", "Proc Result", "Rec Result"]);

        // Helper function to format proc values
        fn format_optional_key<T, F>(opt: Option<&RngMetadatas>, f: F) -> String
        where
            F: FnOnce(&RngMetadatas) -> T,
            T: std::fmt::Display,
        {
            match opt {
                Some(p) => format!("{:.7}", f(p)),
                None => "No Processing".to_string(),
            }
        }

        // Entropy tests
        builder.push_record([
            "Per-Bit Entropy",
            &format!("{:.7}", self.quant.entropy()),
            &format_optional_key(self.proc, |p| p.entropy()),
            &format_optional_key(self.rec, |r| r.entropy()),
        ]);

        builder.push_record([
            "Per-Byte Entropy",
            &format!("{:.7}", self.quant.byte_entropy()),
            &format_optional_key(self.proc, |p| p.byte_entropy()),
            &format_optional_key(self.rec, |r| r.byte_entropy()),
        ]);

        builder.push_record([
            "Per-Bit Min-Entropy",
            &format!("{:.7}", self.quant.min_entropy()),
            &format_optional_key(self.proc, |p| p.min_entropy()),
            &format_optional_key(self.rec, |r| r.min_entropy()),
        ]);

        builder.push_record([
            "Per-Byte Min-Entropy",
            &format!("{:.7}", self.quant.byte_min_entropy()),
            &format_optional_key(self.proc, |p| p.byte_min_entropy()),
            &format_optional_key(self.rec, |r| r.byte_min_entropy()),
        ]);

        // NIST 800-22 statistical tests
        builder.push_record([
            "Frequency",
            &format!(
                "{:.7}",
                self.quant
                    .frequency()
                    .expect("Statistical test failed")
                    .p_value
            ),
            &format_optional_key(self.proc, |p| {
                p.frequency().expect("Statistical test failed").p_value
            }),
            &format_optional_key(self.rec, |r| {
                r.frequency().expect("Statistical test failed").p_value
            }),
        ]);

        let block_size = self.block_size;
        builder.push_record([
            "Block Frequency",
            &format!(
                "{:.7}",
                self.quant
                    .block_frequency(block_size)
                    .expect("Statistical test failed")
                    .p_value
            ),
            &format_optional_key(self.proc, |p| {
                p.block_frequency(block_size)
                    .expect("Statistical test failed")
                    .p_value
            }),
            &format_optional_key(self.rec, |r| {
                r.block_frequency(block_size)
                    .expect("Statistical test failed")
                    .p_value
            }),
        ]);

        builder.push_record([
            "Longest Run Of Ones",
            &format!(
                "{:.7}",
                self.quant
                    .longest_run_of_ones()
                    .expect("Statistical test failed")
                    .p_value
            ),
            &format_optional_key(self.proc, |p| {
                p.longest_run_of_ones()
                    .expect("Statistical test failed")
                    .p_value
            }),
            &format_optional_key(self.rec, |r| {
                r.longest_run_of_ones()
                    .expect("Statistical test failed")
                    .p_value
            }),
        ]);

        builder.push_record([
            "Discrete Fourier Transform",
            &format!(
                "{:.7}",
                self.quant
                    .discrete_fourier_transform()
                    .expect("Statistical test failed")
                    .p_value
            ),
            &format_optional_key(self.proc, |p| {
                p.discrete_fourier_transform()
                    .expect("Statistical test failed")
                    .p_value
            }),
            &format_optional_key(self.rec, |r| {
                r.discrete_fourier_transform()
                    .expect("Statistical test failed")
                    .p_value
            }),
        ]);

        builder.push_record([
            "Non Overlapping Template Matching",
            &format!(
                "{:.7}",
                self.quant
                    .non_overlapping_template_matching(block_size)
                    .expect("Statistical test failed")
                    .p_value
            ),
            &format_optional_key(self.proc, |p| {
                p.non_overlapping_template_matching(block_size)
                    .expect("Statistical test failed")
                    .p_value
            }),
            &format_optional_key(self.rec, |r| {
                r.non_overlapping_template_matching(block_size)
                    .expect("Statistical test failed")
                    .p_value
            }),
        ]);

        builder.push_record([
            "Linear Complexity",
            &format!(
                "{:.7}",
                self.quant
                    .linear_complexity(block_size)
                    .expect("Statistical test failed")
                    .p_value
            ),
            &format_optional_key(self.proc, |p| {
                p.linear_complexity(block_size)
                    .expect("Statistical test failed")
                    .p_value
            }),
            &format_optional_key(self.rec, |r| {
                r.linear_complexity(block_size)
                    .expect("Statistical test failed")
                    .p_value
            }),
        ]);

        builder.push_record([
            "Approximate Entropy",
            &format!(
                "{:.7}",
                self.quant
                    .approximate_entropy(block_size)
                    .expect("Statistical test failed")
                    .p_value
            ),
            &format_optional_key(self.proc, |p| {
                p.approximate_entropy(block_size)
                    .expect("Statistical test failed")
                    .p_value
            }),
            &format_optional_key(self.rec, |r| {
                r.approximate_entropy(block_size)
                    .expect("Statistical test failed")
                    .p_value
            }),
        ]);

        builder.push_record([
            "Cumulative Sums",
            &format!(
                "{:.7}",
                self.quant
                    .cumulative_sum()
                    .expect("Statistical test failed")
                    .p_value
            ),
            &format_optional_key(self.proc, |p| {
                p.cumulative_sum().expect("Statistical test failed").p_value
            }),
            &format_optional_key(self.rec, |r| {
                r.cumulative_sum().expect("Statistical test failed").p_value
            }),
        ]);

        // Build and style the table
        let mut table = builder.build();
        table.with(tabled::settings::Style::modern_rounded());
        table.with(tabled::settings::Panel::horizontal(
            1,
            "                        Entropy Results",
        ));
        table.with(tabled::settings::Panel::horizontal(
            6,
            "                       NIST 800-22 Suite",
        ));
        write!(f, "{table}")
    }
}

#[derive(Debug)]
pub struct KeyGeneration {
    kgr: f32,
    post_processing: Option<KeyPair>,
    pub post_quantization: KeyPair,
    post_reconciliation: Option<KeyPair>,
    host_timings: HostTimings,
    duration_s: f32,
    received_bits: ReceivedBits,
}

#[allow(clippy::similar_names)]
impl KeyGeneration {
    pub const fn new(
        kgr: f32,
        post_processing: Option<KeyPair>,
        post_quantization: KeyPair,
        post_reconciliation: Option<KeyPair>,
        host_timings: HostTimings,
        duration_s: f32,
        received_bits: ReceivedBits,
    ) -> Self {
        Self {
            kgr,
            post_processing,
            post_quantization,
            post_reconciliation,
            host_timings,
            duration_s,
            received_bits,
        }
    }

    #[allow(clippy::cast_precision_loss)]
    pub fn compute_kgr(key_length: usize, elapsed_time: f32) -> f32 {
        key_length as f32 / elapsed_time
    }
}

impl fmt::Display for KeyGeneration {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "\tStart Time (host): {}", self.host_timings.start)?;
        write!(f, "\n\tEnd Time (host): {}", self.host_timings.end)?;
        write!(f, "\n\tTotal Elapsed Time: {:.2}s", self.duration_s)?;
        write!(
            f,
            "\n\tTotal Number Of Received Bytes From:\n\t\tAlice: {} bytes\n\t\tBob: {} bytes",
            self.received_bits.alice, self.received_bits.bob
        )?;
        write!(f, "\n\tKey Generation Rate: {:.2}bps", self.kgr)?;
        write!(f, "\nPost Quantization:\n{}", self.post_quantization)?;

        write!(f, "\nPost Processing: \n")?;
        if let Some(post_processing) = &self.post_processing {
            write!(f, "{post_processing}")?;
        } else {
            write!(f, "{}", String::from("\tNo Key Processing\n").yellow())?;
        }

        write!(f, "\nPost Reconciliation: \n")?;
        if let Some(post_recon) = &self.post_reconciliation {
            write!(f, "{post_recon}")?;
        } else {
            write!(f, "{}", String::from("\tNo Reconciliated Key\n").yellow())?;
        }
        Ok(())
    }
}

#[derive(Debug)]
pub struct KeyGenerationsConf {
    pub is_master: bool,
    csi_type: csi_type_t,
    pre_process_type: preprocess_type_t,
    quant_type: quant_type_t,
    recon_type: recon_type_t,
    probe_delay: usize,
}

impl KeyGenerationsConf {
    pub const fn new(
        is_master: bool,
        csi_type: csi_type_t,
        pre_process_type: preprocess_type_t,
        quant_type: quant_type_t,
        recon_type: recon_type_t,
        probe_delay: usize,
    ) -> Self {
        Self {
            is_master,
            csi_type,
            pre_process_type,
            quant_type,
            recon_type,
            probe_delay,
        }
    }
}

impl fmt::Display for KeyGenerationsConf {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "is_master: {}, csi_type: {:?}, pre_process_type: {:?}, quant_type: {:?}, recon_type: {:?}, probe_delay: {}ms", self.is_master, self.csi_type, self.pre_process_type, self.quant_type, self.recon_type, self.probe_delay)
    }
}

#[derive(Debug)]
pub struct SingleParticipantKeyGenerationsMetadata {
    conf: KeyGenerationsConf,
    pub key_generation: SingleParticipantKeyGenerations,
    start_time: NaiveDateTime,
    end_time: NaiveDateTime,
    elapsed_time: f32,
}

impl SingleParticipantKeyGenerationsMetadata {
    pub const fn new(
        conf: KeyGenerationsConf,
        key_generation: SingleParticipantKeyGenerations,
        start_time: NaiveDateTime,
        end_time: NaiveDateTime,
        elapsed_time: f32,
    ) -> Self {
        Self {
            conf,
            key_generation,
            start_time,
            end_time,
            elapsed_time,
        }
    }
}
impl fmt::Display for SingleParticipantKeyGenerationsMetadata {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "{}", "Time Data:".bold())?;
        writeln!(f, "\tStart Time (host): {}", self.start_time)?;
        writeln!(f, "\tEnd Time (host): {}", self.end_time)?;
        writeln!(f, "\tTotal Elapsed Time: {:.2}s\n", self.elapsed_time)?;

        writeln!(f, "{}", "Parameters:".bold())?;
        writeln!(f, "\t{}", self.conf)?;

        writeln!(f, "\n{}", "RNG:".bold())?;

        writeln!(
            f,
            "\n{}\n{}\n",
            "Key Generations:".italic(),
            self.key_generation
        )
    }
}
#[derive(Debug)]
pub struct KeyGenerationsMetadata {
    confs: (KeyGenerationsConf, KeyGenerationsConf),
    pub key_generations: KeyGenerations,
    start_time: NaiveDateTime,
    end_time: NaiveDateTime,
    elapsed_time: f32,
}

impl KeyGenerationsMetadata {
    pub const fn new(
        confs: (KeyGenerationsConf, KeyGenerationsConf),
        key_generations: KeyGenerations,
        start_time: NaiveDateTime,
        end_time: NaiveDateTime,
        elapsed_time: f32,
    ) -> Self {
        Self {
            confs,
            key_generations,
            start_time,
            end_time,
            elapsed_time,
        }
    }
}

impl fmt::Display for KeyGenerationsMetadata {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "{}", "Time Data:".bold())?;
        writeln!(f, "\tStart Time (host): {}", self.start_time)?;
        writeln!(f, "\tEnd Time (host): {}", self.end_time)?;
        writeln!(f, "\tTotal Elapsed Time: {:.2}s\n", self.elapsed_time)?;

        writeln!(f, "{}", "Parameters:".bold())?;
        writeln!(f, "\tAlice: {}", self.confs.0)?;
        writeln!(f, "\tBob: {}", self.confs.1)?;

        writeln!(f, "\n{}", "RNG:".bold())?;
        let alice_quant = self
            .key_generations
            .aggregate_keys(KeyType::Quantized, Who::Alice)
            .expect("todo");
        let alice_quant_rng_metadatas = RngMetadatas::new(&alice_quant);

        let bob_quant = self
            .key_generations
            .aggregate_keys(KeyType::Quantized, Who::Bob)
            .expect("todo");
        let bob_quant_rng_metadatas = RngMetadatas::new(&bob_quant);
        let alice_and_bob_rng = AliceAndBobTabledRngMetadatas::new(
            &alice_quant_rng_metadatas,
            &bob_quant_rng_metadatas,
            8,
        );
        writeln!(f, "Alice & Bob RNGs: \n{alice_and_bob_rng}")?;

        writeln!(
            f,
            "\n{}\n{}\n",
            "Key Generations:".italic(),
            self.key_generations
        )
    }
}
