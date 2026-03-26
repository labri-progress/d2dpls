#pragma once

#include <stdint.h>

typedef enum __attribute__((__packed__)) {
  CSI_PACKET_RSSI = 0,
  CSI_REGISTER_RSSI,
  CSI_ADJACENT_REGISTER_RSSI,
  CSI_CLSSI,
  CSI_NUM_TYPE
} csi_type_t;

extern int16_t normalize_csi(int16_t csi);
