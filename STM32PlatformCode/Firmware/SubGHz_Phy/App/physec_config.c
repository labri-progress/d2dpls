#include "physec_config.h"
#include "libphysec/libphysec.h"
#include "subghz_phy_app.h"

physec_config physec_conf = {
    .keygen =
        {
            .is_master = IS_MASTER,
            .keygen_id = DEFAULT_KEYGEN_ID,
            .csi_type = CSI_PACKET_RSSI,
            .pre_process_type = PREPROCESS_SAVITSKY_GOLAY,
            .quant_type = QUANT_MBR_LOSSLESS,
            .recon_type = RECON_ECC_SS,
            .probe_padding = PHYSEC_PROBE_PADDING,
            .probe_delay = PHYSEC_PROBE_DELAY,
        },
    .telemetry = {.enabled = true,
                  .logging_enabled = true,
                  .keygen_info_enabled = true},
    .physical_layer = {
        .modulation = PHYSEC_PHY_MODULATION_LORA,
        .lora = {.sf = LORA_SPREADING_FACTOR,
                 .bw = LORA_BANDWIDTH,
                 .power = TX_OUTPUT_POWER},
    }};

bool check_quant_method(uint8_t qtype) {
  switch (qtype) {
  case QUANT_MBE_LOSSY:
  case QUANT_MBR_LOSSLESS:
  case QUANT_SB_DIFF_LOSSY:
  case QUANT_SB_EXCURSION_LOSSY:
  case QUANT_MB_EXCURSION_LOSSY:
  case QUANT_ADAPTIVE:
    return true;
  default:
    break;
  }

  return false;
}

HAL_StatusTypeDef physec_config_write_eeprom(uint8_t offset_address,
                                             physec_config *config) {
  uint32_t eeprom_address = DATA_EEPROM_BASE + offset_address;
  HAL_StatusTypeDef status = HAL_OK;

  /*Here serialize the data*/
  uint8_t buf_size = 16;
  uint8_t buffer[16] = {0};
  uint8_t offset = 0;

  // keygen_config
  buffer[offset++] = config->keygen.is_master;
  buffer[offset++] = config->keygen.keygen_id;
  buffer[offset++] = config->keygen.csi_type;
  buffer[offset++] = config->keygen.pre_process_type;
  buffer[offset++] = config->keygen.quant_type;
  buffer[offset++] = config->keygen.recon_type;
  buffer[offset++] = config->keygen.probe_padding;

  // Attention : on fait du little endian
  buffer[offset++] = (uint8_t)(config->keygen.probe_delay & 0xFF);
  buffer[offset++] = (uint8_t)((config->keygen.probe_delay >> 8) & 0xFF);

  // telemetry_config
  buffer[offset++] =
      *((uint8_t *)&config->telemetry); // 1 octet (bitfields packés)

  // physical_layer_config
  buffer[offset++] = config->physical_layer.modulation;

  // on suppose modulation LORA
  buffer[offset++] = config->physical_layer.lora.sf;
  buffer[offset++] = config->physical_layer.lora.bw;
  buffer[offset++] = config->physical_layer.lora.power;

  // Question si nécessaire de padder les 2 dernierers cases du tableau buffer
  buffer[offset++] = 0x00; // alignement
  buffer[offset++] = 0x00; // alignement

  status = HAL_FLASHEx_DATAEEPROM_Unlock();
  offset = 0;
  while (buf_size-- && status == HAL_OK) {
    status = HAL_FLASHEx_DATAEEPROM_Program(TYPEPROGRAMDATA_BYTE,
                                            eeprom_address, buffer[offset]);
    offset++;
    eeprom_address++;
  }

  HAL_FLASHEx_DATAEEPROM_Lock();
  return status;
}

bool physec_config_read_eeprom(uint8_t offset_address, physec_config *config) {
  uint32_t eeprom_address = DATA_EEPROM_BASE + offset_address;
  uint8_t buf_size = 16;
  uint8_t buffer[16] = {0};
  uint8_t offset = 0;

  while (offset < buf_size) {
    buffer[offset++] = *(__IO uint8_t *)eeprom_address;
    ;
    eeprom_address++;
  }

  // keygen_config
  offset = 0;
  config->keygen.is_master = buffer[offset++];
  config->keygen.keygen_id = buffer[offset++];
  config->keygen.csi_type = buffer[offset++];
  config->keygen.pre_process_type = buffer[offset++];
  config->keygen.quant_type = buffer[offset++];
  config->keygen.recon_type = buffer[offset++];
  config->keygen.probe_padding = buffer[offset++];

  // probe_delay
  config->keygen.probe_delay = (uint16_t)buffer[offset++];
  config->keygen.probe_delay |= (uint16_t)(buffer[offset++] << 8);

  // telemetry_config
  *((uint8_t *)&config->telemetry) =
      buffer[offset++]; // 1 octet (bitfields packés)

  // physical_layer_config
  config->physical_layer.modulation = buffer[offset++];

  // modulation LoRa
  config->physical_layer.lora.sf = buffer[offset++];
  config->physical_layer.lora.bw = buffer[offset++];
  config->physical_layer.lora.power = buffer[offset++];

  // check si config overflow
  if (buffer[offset++] != 0x00 || buffer[offset++] != 0x00) {
    /*padding issue*/
    return false;
  }

  return true;

  /*return config;*/
}
/*HAL_StatusTypeDef HAL_FLASHEx_DATAEEPROM_Unlock();
HAL_StatusTypeDef HAL_FLASHEx_DATAEEPROM_Lock(void);

HAL_StatusTypeDef HAL_FLASHEx_DATAEEPROM_Erase(uint32_t Address);
HAL_StatusTypeDef HAL_FLASHEx_DATAEEPROM_Program(uint32_t TypeProgram, uint32_t
Address, uint32_t Data);*/
