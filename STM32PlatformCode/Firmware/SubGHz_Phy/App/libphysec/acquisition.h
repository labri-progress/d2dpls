#pragma once

#include "types.h"
#include <stdint.h>

typedef enum __attribute__((__packed__)) {
  CSI_PACKET_RSSI = 0,
  CSI_REGISTER_RSSI = 1 | NOT_IMPLEMENTED,
  CSI_ADJACENT_REGISTER_RSSI = 2 | NOT_IMPLEMENTED,
  CSI_CLSSI = 3 | NOT_IMPLEMENTED,
  CSI_ECDH,
  CSI_NUM_TYPE = 4
} csi_type_t;

int16_t normalize_csi(int16_t csi);
