#include "reconciliation.h"
#include "physec_utils.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

static void (*log_msg_fn)(const char *fmt, va_list args) = NULL;

void set_log(void (*fn)(const char *fmt, va_list args)) { log_msg_fn = fn; }

void log_msg(const char *fmt, ...) {
  if (log_msg_fn != NULL) {
    va_list args;
    va_start(args, fmt);
    log_msg_fn(fmt, args);
    va_end(args);
  }
}

void byte_array_and(uint8_t *a, uint32_t a_size, uint8_t *b, uint32_t b_size) {
  if (a_size < b_size) {
    FAIL("a_size must be >= b_size");
  }
  for (uint32_t idx = 0; idx < a_size && idx < b_size; idx++) {
    a[idx] = a[idx] & b[idx];
  }
}
void byte_array_xor(uint8_t *a, uint32_t a_size, uint8_t *b, uint32_t b_size) {
  if (a_size < b_size) {
    FAIL("a_size must be >= b_size");
  }
  for (uint32_t idx = 0; idx < b_size; idx++) {
    a[idx] = a[idx] ^ b[idx];
  }
}

bool has_n_padding_zeros(uint8_t *barr, uint32_t barr_size, uint32_t n) {
  if (n > barr_size) {
    return false;
  }
  for (uint32_t idx = barr_size - n; idx < barr_size; idx++) {
    if (barr[idx])
      return false;
  }
  return true;
}

void byte_array_copy_bytes(uint8_t *out, uint8_t *from, uint32_t n) {
  for (uint32_t idx = 0; idx < n; idx++) {
    out[idx] = from[idx];
  }
}
