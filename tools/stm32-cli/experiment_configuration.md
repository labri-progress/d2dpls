# Experiment Config Files

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

- `CSI_PACKET_RSSI`: 0
- `CSI_REGISTER_RSSI`: 1
- `CSI_ADJACENT_REGISTER_RSSI`: 2
- `CSI_CLSSI`: 3
- `CSI_NUM_TYPE`: 4

### Supported pre-processing:

Processing method applied to acquisition data before quantization; if any.

`pre_process_type` (uint8)

- `PREPROCESS_NONE`: 0
- `PREPROCESS_SAVITSKY_GOLAY`: 1
- `PREPROCESS_KALMAN`: 2
- `PREPROCESS_RANDOM_WAYPOINT_MODEL`: 3
- `PREPROCESS_NUM_TYPE`: 4

### Supported quantization methods:

Supported quantization methods, ie. the method used to generate bits from the CSI samples.

`quant_type` (uint8)

- `QUANT_MBR_LOSSLESS`: 0
- `QUANT_MBE_LOSSY`: 6
- `QUANT_SB_DIFF_LOSSY`: 9
- `QUANT_SB_EXCURSION_LOSSY`: 13
- `QUANT_MB_EXCURSION_LOSSY`: 17
- `QUANT_ADAPTIVE`: 23
- `QUANT_SB_LOSSLESS`: 24
- `QUANT_SB_LOSSLESS_BLOCKWISE`: 26
- `QUANT_SB_LOSSY`: 29
- `QUANT_SB_LOSSY_BLOCKWISE`: 31

### Supported information-reconciliation methods:

`recon_type` (uint8)

- `RECON_ECC_SS`: 0
- `RECON_FE_STL`: 1
- `RECON_PCS`: 2
- `RECON_NUM_TYPE`: 3

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

`tx_power` (uint8)