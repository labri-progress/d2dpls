#pragma once

#include <stdint.h>

#define ALIGNED_MEMSIZE_CSI_T(size) ((((size)) / sizeof(csi_t)) * sizeof(csi_t))

#define NUM_CSI_T_FROM_MEMSIZE(memsize) ((memsize) / sizeof(csi_t))

// Type for holding an index of a quantized csi.
typedef uint32_t quant_index_t;

// Type for holding a Channel State Information (CSI) value.
#if defined(__PHYSEC_SX1276__)
// RSSI range is [[ -164, 0 ]]
#define RSSI_MIN 0
#define RSSI_MAX -164
typedef int16_t csi_t;
#elif defined(__PHYSEC_SX1272__)
// RSSI range is [[ -139, 0 ]]
#define RSSI_MIN 0
#define RSSI_MAX -139
typedef int16_t csi_t;
#elif defined(__PHYSEC_SX126x__)
// RSSI range is [[ -139, 0 ]]
#define RSSI_MIN 0
#define RSSI_MAX -139
typedef int16_t csi_t;
#else
#define RSSI_MIN 0
#define RSSI_MAX -255
typedef int16_t csi_t;
#endif

#define NOT_IMPLEMENTED 1<<7

typedef struct physec_prng_s physec_prng_t;

struct density {
  csi_t q_0;
  uint16_t bin_nbr;
  csi_t *bins;
  double *values;
};

extern struct density *alloc_density(uint16_t bin_nbr);
extern void free_density(struct density *d);

extern void get_random_bytes(uint8_t *buf, uint32_t buf_size,
                             physec_prng_t *prng);
extern void get_random_sampling_mask(uint8_t *mask_buf, uint32_t mask_size,
                                     uint32_t hot_bits, physec_prng_t *prng);
extern void prf(uint8_t *out_buf, uint32_t out_size, uint8_t *key_buf,
                uint32_t key_size, uint8_t *nonce, uint32_t nonce_size,
                physec_prng_t *prng);
