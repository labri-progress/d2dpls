#pragma once

#include "fuzzy_extractor_sample_then_lock.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum __attribute__((__packed__)) {
  // Error Correcting Codes + Secure Sketch
  RECON_ECC_SS = 0,
  // Fuzzy Extractors, Sample Then Lock construct
  RECON_FE_STL = 1,
  // Perturbed Compressed Sensing
  RECON_PCS = 2,
  RECON_NUM_TYPE
} recon_type_t;

void set_log(void (*fn)(const char *fmt, va_list args));
void log_msg(const char *fmt, ...);

/**
 * fills previously allocated `buf` with `buf_size` random bytes.
 */

void byte_array_and(uint8_t *a, uint32_t a_size, uint8_t *b, uint32_t b_size);
void byte_array_xor(uint8_t *a, uint32_t a_size, uint8_t *b, uint32_t b_size);
void byte_array_copy_bytes(uint8_t *out, uint8_t *from, uint32_t n);
bool has_n_padding_zeros(uint8_t *barr, uint32_t barr_size, uint32_t n);
