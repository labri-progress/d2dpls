#include "physec_utils.h"

#include "cmox_crypto.h"
#include "libphysec/libphysec.h"
#include "subghz_phy_app.h"
#include "usart.h"
#include "utilities_conf.h"
#include "utilities_def.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
void prng_init(physec_prng_t *prng, char *name, uint32_t name_len) {
  prng->hrng.Instance = RNG;
  prng->last_status = HAL_RNG_Init(&prng->hrng);
  prng->name = name;
  prng->name_len = name_len;
  if (prng->last_status != HAL_OK) {
    FAIL("failed initializing");
  }
}
void get_random_bytes(uint8_t *buf, uint32_t buf_size, physec_prng_t *prng) {
  if (prng->hrng.State != HAL_RNG_STATE_READY) {
    FAIL("prng not ready");
  }
  uint32_t random_uint;
  uint32_t remaining = buf_size;
  while (remaining) {
    prng->last_status = HAL_RNG_GenerateRandomNumber(&prng->hrng, &random_uint);
    if (prng->last_status != HAL_OK) {
      FAIL("failed to generate random number");
    }
    uint32_t n_bytes_to_do = MIN(remaining, 4);
    memcpy(buf, &random_uint, n_bytes_to_do);
    remaining -= n_bytes_to_do;
    buf += n_bytes_to_do;
  }
}

void get_random_sampling_mask(uint8_t *mask_buf, uint32_t mask_size,
                              uint32_t hot_bits, physec_prng_t *prng) {
  if (hot_bits > mask_size * 8) {
    FAIL("impossible mask");
  }
  if (hot_bits == mask_size * 8) {
    memset(mask_buf, 0xFF, mask_size);
    return;
  }
  memset(mask_buf, 0, mask_size);
  if (hot_bits == 0)
    return;
  uint32_t remaining = hot_bits;
  uint32_t random_uint;
  while (remaining) {
    prng->last_status = HAL_RNG_GenerateRandomNumber(&prng->hrng, &random_uint);
    if (prng->last_status != HAL_OK) {
      FAIL("failed to generate random number");
    }
    random_uint %= (mask_size * 8);
    uint32_t byte_idx = random_uint / 8;
    uint32_t bit_idx_in_byte = random_uint % 8;
    if (!(mask_buf[byte_idx] & (1 << bit_idx_in_byte))) {
      mask_buf[byte_idx] |= (1 << bit_idx_in_byte);
      remaining--;
    }
  }
}

uint32_t popcnt(uint8_t *barr, uint32_t size) {
  uint32_t sum = 0;
  for (uint32_t i = 0; i < size; i++) {
    sum += __builtin_popcount(barr[i]);
  }
  return sum;
}

void cmox_init() {
  cmox_init_arg_t init_target = {CMOX_INIT_TARGET_AUTO, NULL};
  if (cmox_initialize(&init_target) != CMOX_INIT_SUCCESS) {
    FAIL("failed to initialize cmox");
  }
}

void prf(uint8_t *out_buf, uint32_t out_size, uint8_t *key_buf,
         uint32_t key_size, uint8_t *nonce, uint32_t nonce_size,
         physec_prng_t *prng) {
  cmox_mac_retval_t retval;
  uint32_t computed_size;

  retval = cmox_mac_compute(CMOX_KMAC_128_ALGO, nonce, nonce_size, key_buf,
                            key_size, prng->name, prng->name_len, out_buf,
                            out_size, &computed_size);

  if (computed_size != out_size) {
    tm_plog(TS_ON, VLEVEL_M, "expected %d bytes but got %d", out_size,
            computed_size);
    FAIL("error in prf");
  }
}

/*!
 *	Get hamming weight of a quantization index bitmap.
 */
uint8_t bitmap_hamming_weight(lossy_chunk_bitmap_t bitmap) {
  uint8_t weight = 0;
  for (size_t i = 0; i < MAX_LOSSY_CHUNKS; i++) {
    if (bitmap & (1 << i))
      weight++;
  }
  tm_plog(TS_OFF, VLEVEL_L, "bitmap = 0x%x\n\r", bitmap);
  tm_plog(TS_OFF, VLEVEL_L, "bitmap weight = %d\n\r", weight);
  return weight;
}

void hexlify(uint8_t *buf, size_t size, char *msg, size_t msgsize) {
  if (msgsize < size * 2 + 1) {
    return;
  }
  for (size_t i = 0; i < size; i++) {
    snprintf(msg + i * 2, msgsize - i * 2, "%02x", buf[i]);
  }
}

/* USER CODE BEGIN PrFD */
/*!
 *	Helper for quantizing key bits from CSIs.
 *
 */
int pre_process_and_quantize(void) {

  csi_t tmp_csi[NUM_MAX_CSI] = {0};
  uint8_t key_chunk[KEY_CAPACITY_IN_BYTES] = {0};
  bool lossy_quant = QUANT_IS_LOSSY(physec_conf.keygen.quant_type);
  bool blockwise_quant = QUANT_IS_BLOCKWISE(physec_conf.keygen.quant_type);
  bool excursion_quant = QUANT_IS_EXCURSION(physec_conf.keygen.quant_type);

  size_t num_total_information_bits = 0;
  csi_t *measures_start = csi_measures;
  quant_index_t *indexes_start = indexes;
  size_t cur_num_indexes = NUM_MAX_QUANT_INDEX;
  // for non-blockwise quantization, we do only one iteration
  // with the block being the whole CSI array
  size_t blocksize = num_csi;
  size_t num_rem_csi = num_csi;
  size_t num_blocks = 1;

  if (blockwise_quant) {
    // some measurements are already quantized,
    // indexes array may not be empty
    num_rem_csi = num_csi - num_quantized_csi;
    if (QUANT_MOVING_WINDOW_SIZE > num_rem_csi) {
      tm_plog(TS_ON, VLEVEL_L, "> Not enough measures for blockwise quant\n\r");
      return -1;
    }
    measures_start += num_quantized_csi;
    indexes_start += num_indexes;
    cur_num_indexes -= num_indexes;
    blocksize = QUANT_MOVING_WINDOW_SIZE;
    num_blocks = num_rem_csi / blocksize; // we do not quantize remaining
    // measures which cannot form a block
  } else {
    // non blockwise quant applies to all measures
    // thus wipe physec_key and quant indexes array
    num_quantized_csi = 0;
    num_indexes = 0;
    physec_key_num_bits = -1;
  }

  // avoid copying if not enough measures for blockwise quant
  memcpy(tmp_csi, measures_start, num_rem_csi * sizeof(csi_t));

  measures_start = tmp_csi;

  bool no_pre_process = false;
  switch (physec_conf.keygen.pre_process_type) {
  case PREPROCESS_SAVITSKY_GOLAY:
    if (num_rem_csi < SAVITSKY_GOLAY_WINDOW_SIZE)
      return -1;
    pre_process_savitsky_golay(measures_start, num_rem_csi);
    break;
  case PREPROCESS_RANDOM_WAYPOINT_MODEL:
    if (num_csi < RWM_NEEDED_NUM_CSI)
      return -1;
    break;
  default:
  case PREPROCESS_NONE:
    no_pre_process = true;
    break;
  }
  if (!no_pre_process) {
    // send filtered csis
    tm_send_csis(measures_start, num_rem_csi);
  }

  size_t cur_num_information_bits = 0;
  for (size_t i = 0; i < num_blocks; i++) {
    int ret = -1;
    if (physec_key_num_bits >= KEY_CAPACITY_IN_BYTES * 8) {
      tm_plog(TS_ON, VLEVEL_L, "> Key is full. Stopping quantization.\n\r");
      goto quant_wrapper_end;
    }
    switch (physec_conf.keygen.quant_type) {
    case QUANT_SB_DIFF_LOSSY:
      ret = quant_sb_diff_lossy(measures_start, blocksize, key_chunk,
                                sizeof(key_chunk), indexes_start,
                                &cur_num_indexes);
      num_indexes += cur_num_indexes;
      cur_num_information_bits = (ret - cur_num_indexes);
      break;

    case QUANT_SB_EXCURSION_LOSSY:
      ret = quant_sb_excursion_quantize2(measures_start, blocksize, indexes,
                                         &cur_num_indexes, QUANT_SB_EXCURSION_M,
                                         QUANT_SB_EXCURSION_ALPHA, key_chunk,
                                         sizeof(key_chunk));
      if (physec_conf.keygen.is_master) {
        quant_excursion_select_random_subset(indexes, &cur_num_indexes);
      }
      cur_num_information_bits = cur_num_indexes;
      num_indexes += cur_num_indexes;
      break;

    case QUANT_MB_EXCURSION_LOSSY:
      ret = quant_mb_excursion_quantize2(
          &quant_params, measures_start, blocksize, indexes, &cur_num_indexes,
          QUANT_SB_EXCURSION_M, key_chunk, sizeof(key_chunk));
      if (physec_conf.keygen.is_master) {
        quant_excursion_select_random_subset(indexes, &cur_num_indexes);
      }
      cur_num_information_bits =
          cur_num_indexes * quant_params.nbits_per_sample;
      num_indexes += cur_num_indexes;
      break;

    case QUANT_MBE_LOSSY:
      cur_num_indexes = 0;
      tm_plog(TS_ON, VLEVEL_L, "LoRa-Key\n\r");
      ret = quant_mbe_lossy(measures_start, blocksize, QUANT_MBE_LOSSY_ALPHA,
                            key_chunk, AES_KEY_SIZE_IN_BYTES, NULL,
                            &cur_num_indexes); // we fetch num dropped
      // csis without filling indexes array
      tm_plog(TS_ON, VLEVEL_L, "LoRa-Key Dropped => %d\r\n", cur_num_indexes);
      cur_num_information_bits = ret;

      break;

    case QUANT_MBR_LOSSLESS:
      ret = quant_mbr_lossless(measures_start, blocksize, key_chunk,
                               AES_KEY_SIZE_IN_BYTES);
      cur_num_information_bits = ret;
      break;
    case QUANT_SB_LOSSLESS: // same call
    case QUANT_SB_LOSSLESS_BLOCKWISE:
      ret = quant_sb_lossless(measures_start, blocksize, key_chunk,
                              AES_KEY_SIZE_IN_BYTES);
      cur_num_information_bits += ret;
      break;
    case QUANT_SB_LOSSY:
    case QUANT_SB_LOSSY_BLOCKWISE:
      ret = quant_sb_lossy(measures_start, blocksize, key_chunk,
                           sizeof(key_chunk), indexes_start, &cur_num_indexes);
      num_indexes += cur_num_indexes;
      cur_num_information_bits = (ret - cur_num_indexes);
      break;
    case QUANT_ADAPTIVE:
    default:
      break;
    }
    if ((lossy_quant || excursion_quant) && blockwise_quant) {
      // normalize blockwise indedexes
      for (size_t i = num_indexes - cur_num_indexes; i < num_indexes; i++) {
        indexes[i] += num_quantized_csi;
      }
    }
    if (ret < 0 || (lossy_quant && num_indexes >= NUM_MAX_QUANT_INDEX)) {
      tm_plog(TS_ON, VLEVEL_L, "> Quantization failed\n\r");
      return -1;
    }
    size_t whole = 0;
    if (physec_key_num_bits > 0) {
      whole = physec_key_num_bits;
    }
    if (whole + ret > KEY_CAPACITY_IN_BYTES * 8) {
      ret = KEY_CAPACITY_IN_BYTES * 8 - whole;
    }

    // Add quantized bits to the physec_key (for blockwise quantization)
    // or update the entire physec_key (for non-blockwise quantization)

    if (blockwise_quant) {
      add_quantized_bits_to_physec_key(key_chunk, ret);
      tm_plog(TS_ON, VLEVEL_L,
              "> From (%d measures) => %d bits added to physec_key => current "
              "physec_key size = %d bits \n\r",
              blocksize, cur_num_information_bits,
              cur_num_information_bits + physec_key_num_bits);

    } else {
      update_physec_key(key_chunk, ret);
      tm_plog(TS_ON, VLEVEL_L, "> Current physec_key size = %d bits \n\r",
              physec_key_num_bits);
    }
    if (excursion_quant) {
      num_total_information_bits = num_indexes;
    } else {
      num_total_information_bits = physec_key_num_bits - num_indexes;
    }
    tm_plog(TS_ON, VLEVEL_L, "> Num total information bits = %d\n\r",
            num_total_information_bits);
    measures_start += blocksize;
    indexes_start += cur_num_indexes;
    cur_num_indexes = NUM_MAX_LOSSY - num_indexes;
    num_quantized_csi += blocksize;
  }

// This part is exc only when the physec key size is bigger or eqauls 128bits
quant_wrapper_end:
  if (physec_key_num_bits > 128) {
    tm_plog(TS_ON, VLEVEL_L, "physec_key before truncation = [ ");
    for (size_t i = 0; i < AES_KEY_SIZE_IN_BYTES; i++) {
      if (i == AES_KEY_SIZE_IN_BYTES - 1) {
        tm_plog(TS_OFF, VLEVEL_L, "%02x ]\n\r", physec_key[i]);
      } else {
        tm_plog(TS_OFF, VLEVEL_L, "%02x,", physec_key[i]);
      }
    }
    tm_plog(TS_ON, VLEVEL_L, "> Key is full. Stopping quantization.\n\r");
    truncate_physec_key(physec_key_num_bits); // Truncate the physec_key
    tm_plog(TS_ON, VLEVEL_L, "Truncated physec_key = [ ");
    for (size_t i = 0; i < AES_KEY_SIZE_IN_BYTES; i++) {
      if (i == AES_KEY_SIZE_IN_BYTES - 1) {
        tm_plog(TS_OFF, VLEVEL_L, "%02x ]\n\r", physec_key[i]);
      } else {
        tm_plog(TS_OFF, VLEVEL_L, "%02x,", physec_key[i]);
      }
    }
    physec_key_num_bits = 128;
  }
  if (excursion_quant) {
    // num_indexes is the number of information bits
    return num_indexes;
  }
  return physec_key_num_bits -
         num_indexes; // only returns the number of information bits
}

/*!
 *  Update PHYsec key by adding new bits from a bitkey chunk.
 */
void add_quantized_bits_to_physec_key(uint8_t *key_chunk, size_t chunksize) {
  if (physec_key_num_bits == -1) {
    add_bits_to_key(physec_key, KEY_CAPACITY_IN_BYTES * 8, 0, key_chunk,
                    chunksize);
    physec_key_num_bits = chunksize;
  } else {
    // concatenate key chunk to global key
    add_bits_to_key(physec_key, KEY_CAPACITY_IN_BYTES * 8, physec_key_num_bits,
                    key_chunk, chunksize);
    physec_key_num_bits += chunksize;
  }
}
void update_physec_key(uint8_t *new_physec_key,
                       size_t size_new_physec_key_in_bits) {
  memset(physec_key, 0, KEY_CAPACITY_IN_BYTES); // Clear the old key
  memcpy(physec_key, new_physec_key,
         size_new_physec_key_in_bits); // Copy in the new key
  physec_key_num_bits = size_new_physec_key_in_bits;
}

/* void truncate_physec_key(size_t physec_key_size_in_bits)
{
    const size_t KEEP_BITS  = 128;
    const size_t KEEP_BYTES = KEEP_BITS / 8;

    // If already 128 bits or smaller, nothing to do
    if (physec_key_size_in_bits <= KEEP_BITS)
        return;

    uint8_t out[KEEP_BYTES];
    memset(out, 0, KEEP_BYTES);

    for (size_t bit = 0; bit < KEEP_BITS; bit++) {
        size_t src_byte = bit / 8;
        size_t src_bit  = 7 - (bit % 8);

        size_t dst_byte = bit / 8;
        size_t dst_bit  = 7 - (bit % 8);

        uint8_t bit_val = (physec_key[src_byte] >> src_bit) & 0x01;
        out[dst_byte] |= bit_val << dst_bit;
    }

    // Replace physec_key with the truncated key

    memset(physec_key, 0, KEY_CAPACITY_IN_BYTES);
    memcpy(physec_key, out, KEEP_BYTES);


    physec_key_size_in_bits = KEEP_BITS;
}
*/
// This function is used to only keep the last 128 bits of physec_key

void truncate_physec_key(size_t physec_key_size_in_bits) {
  const size_t KEEP_BITS = 128;
  const size_t KEEP_BYTES = KEEP_BITS / 8;
  // If the key is already 128 bits or smaller, do nothing
  if (physec_key_size_in_bits <= KEEP_BITS)
    return;

  size_t bit_offset =
      physec_key_size_in_bits - KEEP_BITS; // How many bits to skip at the start
                                           // to keep only last 128 bits
  size_t byte_offset = bit_offset / 8; // How many whole bytes we can skip
  size_t shift = bit_offset % 8;       //  shift "shift" bits inside first byte

  uint8_t out[KEEP_BYTES];

  for (size_t i = 0; i < KEEP_BYTES; i++) {
    uint8_t b1 = physec_key[byte_offset + i]
                 << shift; // lowest bits of physec_key[byte_offset + i] to
                           // higher bits of b1.
    uint8_t b2 = 0;

    if (shift != 0)
      b2 = physec_key[byte_offset + i + 1] >>
           (8 - shift); // higest bits of next byte to the lowest bits of b2
    // Combine bits
    out[i] = b1 | b2;
  }

  // physec_key truncated
  memset(physec_key, 0, KEY_CAPACITY_IN_BYTES); // Clear the old key
  memcpy(physec_key, out, KEEP_BYTES);
}

/*!
 *	\brief Use vigenere algorithm (not secured) for encryption/decryption
 *PoC with PHYsec keys.
 */
void vigenere_encrypt_decrypt(uint8_t *buf, size_t bufsize, const uint8_t *key,
                              size_t keysize) {
  for (size_t i = 0; i < bufsize; i++) {
    buf[i] ^= key[i % keysize];
  }
}

/*!
 *	\brief UART Rx IRQ Callback (streaming mode)
 *
 *	Allows receiving and parsing configuration packets from the host.
 *
 *	packet parsing is performed in 2 stages. First, the packet header
 *	is parsed to retrieve the expected packet size. Then, we wait for
 *	receiving this amount of bytes before parsing the payload.
 *
 *	In case of variable size payloads, the callback waits for an additionnal
 *	header indicating payload size (e.g for loading CSIs).
 *
 *	This Callback can only handle sequentially written packets. If invalid
 *data is written by Host to UART during the configuration time window, it can
 *break the parsing logic.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart != &huart2)
    return;
  if (uart_bufsize < 256) {
    uart_buf[uart_bufsize++] = uart_rx_buf[0];
  }
  if (cfg_type == 0xff) {
    // First stage: parse header to determine expected packet size
    if (uart_bufsize >= sizeof(physec_config_packet_t)) {
      physec_config_packet_t *pkt = (physec_config_packet_t *)uart_buf;
      if (pkt->magic != UART_PHYSEC_CONFIG_MAGIC) {
        uart_bufsize = 0;
        goto uart_go_to_rx;
      }

      switch (pkt->config_type) {
      case PHYSEC_CONFIG_START:
        on_config = true;
        uart_bufsize = 0;
        goto uart_go_to_rx;
      case PHYSEC_CONFIG_KEYGEN:
        expected = sizeof(physec_keygen_config);
        break;
      case PHYSEC_CONFIG_TELEMETRY:
        expected = sizeof(physec_telemetry_config);
        break;
      case PHYSEC_CONFIG_RADIO:
        expected = sizeof(physec_physical_layer_config);
        break;
      case PHYSEC_CONFIG_LOAD_CSIS:
        expected = sizeof(physec_load_csi);
        break;
      case PHYSEC_CONFIG_DONE:
        config_done = true;
      default:
        goto uart_go_to_rx;
      }
      cfg_type = pkt->config_type;
      uart_bufsize = 0;
    }
  } else {
    if (uart_bufsize >= expected) {
      // second stage: if enough bytes have been received, parse the payload
      uart_bufsize = 0;
      switch (cfg_type) {
      case PHYSEC_CONFIG_KEYGEN:
        memcpy(&(physec_conf.keygen), uart_buf, sizeof(physec_keygen_config));
        break;
      case PHYSEC_CONFIG_TELEMETRY:
        memcpy(&(physec_conf.telemetry), uart_buf,
               sizeof(physec_telemetry_config));
        break;
      case PHYSEC_CONFIG_RADIO:
        memcpy(&(physec_conf.physical_layer), uart_buf,
               sizeof(physec_physical_layer_config));
        break;
      case PHYSEC_CONFIG_LOAD_CSIS:
        if (num_expected_csi == 0) {
          physec_load_csi *load_csi = (physec_load_csi *)uart_buf;
          num_expected_csi = load_csi->num_csi;
          if (num_expected_csi > NUM_MAX_CSI) {
            num_expected_csi = NUM_MAX_CSI;
          }
          expected = sizeof(csi_t);
          goto uart_go_to_rx;
        } else {
          csi_t csi = *(csi_t *)uart_buf;
          csi_measures[num_csi++] = csi;
          if (num_csi < num_expected_csi) {
            goto uart_go_to_rx;
          }
          num_expected_csi = 0;
        }
        break;
      default:
        break;
      }
      cfg_type = 0xff;
    }
  }
uart_go_to_rx:
  HAL_UART_Receive_DMA(&huart2, uart_rx_buf, 1);
}
