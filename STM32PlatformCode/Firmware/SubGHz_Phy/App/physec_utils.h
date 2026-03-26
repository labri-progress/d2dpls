#ifndef __PHYSEC_UTILS_H__
#define __PHYSEC_UTILS_H__
#include "libphysec/libphysec.h"
#include "physec_config.h"
#include "stm32l0xx_hal_rng.h"
#include "subghz_phy_app.h"
#include "utilities_conf.h"

#include <stdint.h>

#define MIN(a, b) a > b ? b : a;
#define FAIL(msg)                                                              \
  tm_plog(TS_ON, VLEVEL_M, "%s (%s:%d)\r\n", msg, __FILE__, __LINE__);         \
  while (1)                                                                    \
    ;

/* === Useful Methods === */
uint8_t bitmap_hamming_weight(lossy_chunk_bitmap_t bitmap);
void hexlify(uint8_t *buf, size_t size, char *msg, size_t msgsize);
int pre_process_and_quantize(void);
void add_quantized_bits_to_physec_key(
    uint8_t *key_chunk,
    size_t chunksize); // This function is used to add a number of quantized
                       // bits from a set of measurements (equal to the window
                       // size) in cases where the quantization is blockwise.

void update_physec_key(
    uint8_t *new_physec_key,
    size_t size_new_physec_key_in_bits); // This function is used to update the
                                         // physec_key in the case of
                                         // non-blockwise quant, where all
                                         // measurements are quantized every
                                         // time a new measurement is added.

void truncate_physec_key(size_t physec_key_size_in_bits);

void vigenere_encrypt_decrypt(uint8_t *buf, size_t bufsize, const uint8_t *key,
                              size_t keysize);
/* === Platform Global Variables === */

extern physec_config physec_conf;
extern csi_t csi_measures[NUM_MAX_CSI];
extern quant_index_t indexes[NUM_MAX_QUANT_INDEX];
extern size_t num_csi;
extern size_t num_quantized_csi;
extern size_t num_indexes;
extern int16_t physec_key_num_bits;
extern quant_lossy_params_t quant_params;
extern uint8_t physec_key[KEY_CAPACITY_IN_BYTES];

extern size_t uart_bufsize;
extern uint8_t uart_buf[256];
extern uint8_t cfg_type;
extern size_t expected;
extern size_t num_expected_csi;
extern bool on_config;
extern bool config_done;
extern uint8_t uart_rx_buf[MAX_UART_BUF_SIZE];

typedef struct physec_prng_s {
  HAL_StatusTypeDef last_status;
  RNG_HandleTypeDef hrng;
  uint8_t *name;
  uint32_t name_len;
} physec_prng_t;

#endif
