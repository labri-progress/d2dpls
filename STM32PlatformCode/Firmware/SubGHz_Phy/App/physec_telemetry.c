#include "physec_telemetry.h"
#include "rtc_if.h"
#include "subghz_phy_app.h"
#include "usart.h"
#include "usart_if.h"
#include <stdio.h>

#include "physec_config.h"

#include "libphysec/types.h"

size_t physec_telemetry_get_size(physec_telemetry_packet_t *packet) {
  size_t payload_size = 0;
  physec_telemetry_keygen_info_t *kg_info;
  switch (packet->type) {
  case PHYSEC_TELEMETRY_KG_CONF:
    payload_size = sizeof(physec_keygen_config);
    break;
  case PHYSEC_TELEMETRY_KG_INFO:
    kg_info = (physec_telemetry_keygen_info_t *)&(packet->data);
    size_t num_bytes = (kg_info->num_bits + 8 - 1) / 8;
    payload_size =
        sizeof(kg_info->key_type) + sizeof(kg_info->num_bits) + num_bytes;
    break;
  case PHYSEC_TELEMETRY_LOGGING: {
    physec_telemetry_logging_t *logging_payload =
        (physec_telemetry_logging_t *)&(packet->data);
    payload_size =
        sizeof(logging_payload->size) + logging_payload->size * sizeof(uint8_t);
    break;
  }
  case PHYSEC_TELEMETRY_CSIS: {
    physec_telemetry_csi_t *csi_payload =
        (physec_telemetry_csi_t *)&(packet->data);
    payload_size =
        sizeof(csi_payload->num_csi) + csi_payload->num_csi * sizeof(csi_t);
    break;
  }
  default:
    break;
  }
  return sizeof(physec_telemetry_packet_t) + payload_size;
}
/*!
 *	\brief printf-like function for logging messages TO UART
 *	using telemetry packets.
 */
int physec_telemetry_log(bool ts, const char *fmt, ...) {
  if (!physec_conf.telemetry.enabled || !physec_conf.telemetry.logging_enabled)
    return 0;

  // carefull here, it works because compiler can determine
  // hdr_size
  size_t hdr_size =
      sizeof(physec_telemetry_packet_t) + sizeof(physec_telemetry_logging_t);
  uint8_t buffer[LOG_MSG_MAX_SIZE + hdr_size];
  va_list args;
  size_t len;

  physec_telemetry_packet_t *pkt = (physec_telemetry_packet_t *)buffer;
  pkt->magic = UART_PHYSEC_TELEMETRY_MAGIC;
  pkt->type = PHYSEC_TELEMETRY_LOGGING;
  physec_telemetry_logging_t *log = (physec_telemetry_logging_t *)&(pkt->data);

  va_start(args, fmt);
  // if we need to add timestamp informations, we simply add it before the
  // formatted string
  if (ts) {
    uint8_t fmt_buffer[LOG_MSG_MAX_SIZE];
    uint32_t seconds;
    uint16_t subseconds;
    seconds = RTC_IF_GetTime(&subseconds);
    snprintf((char *)fmt_buffer, LOG_MSG_MAX_SIZE, "%lu.%u %s", seconds,
             subseconds, fmt);
    len = vsnprintf((char *)buffer + hdr_size, sizeof(buffer) - hdr_size,
                    (char *)fmt_buffer, args);
  } else {
    len = vsnprintf((char *)buffer + hdr_size, sizeof(buffer) - hdr_size, fmt,
                    args);
  }
  va_end(args);

  if (len > 0) {
    // Ensure the message is not longer than the buffer
    if (len > sizeof(buffer) - 1) {
      len = sizeof(buffer) - 1;
    }

    log->size = len;

    // Transmit the formatted string over UART
    HAL_UART_Transmit(&huart2, (uint8_t *)pkt, physec_telemetry_get_size(pkt),
                      200);
  }

  return len; // Return the number of characters transmitted
}

/*!
 *	\brief Send CSIs to UARt using Telemetry packet.
 */
// void physec_telemetry_send_csis_stub(csi_t *csis, size_t num) {
//   uint8_t *buf = NULL;
//   size_t num_bytes = num * sizeof(csi_t);
//   __asm__(
//     "sub SP, SP, %0\n\t"
//     "mov %1, SP\n\t"
//     : : "r" (num_bytes), "r" (buf) : "memory"
//   );
// }
bool tx_done = false;
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart == &huart2) {
    tx_done = true;
  }
}

// void physec_telemetry_send_csis(csi_t *csis, size_t num) {
//   uint8_t buf[3032] = {0};
//   physec_telemetry_packet_t *pkt = (physec_telemetry_packet_t *)buf;
//   pkt->magic = UART_PHYSEC_TELEMETRY_MAGIC;
//   pkt->type = PHYSEC_TELEMETRY_CSIS;
//   physec_telemetry_csi_t *csi_pkt = (physec_telemetry_csi_t *)&(pkt->data);
//   csi_pkt->num_csi = num;
//   memcpy(csi_pkt->csis, csis, sizeof(csi_t) * num);
//   HAL_UART_Transmit(&huart2, pkt, physec_telemetry_get_size(pkt), 1000);
// }

void physec_telemetry_send_csis(csi_t *csis, size_t num) {
  uint8_t buf[64] = {0};
  physec_telemetry_packet_t *pkt = (physec_telemetry_packet_t *)buf;
  pkt->magic = UART_PHYSEC_TELEMETRY_MAGIC;
  pkt->type = PHYSEC_TELEMETRY_CSIS;
  physec_telemetry_csi_t *csi_pkt = (physec_telemetry_csi_t *)&(pkt->data);
  csi_pkt->num_csi = num;
  size_t num_csi_in_chunk = sizeof(buf) / sizeof(csi_t);
  size_t headers_size =
      sizeof(physec_telemetry_packet_t) + sizeof(physec_telemetry_csi_t);
  HAL_UART_Transmit(&huart2, (uint8_t *)pkt, headers_size, 200);
  for (size_t i = 0; i < num; i += num_csi_in_chunk) {
    if (num - i < num_csi_in_chunk)
      num_csi_in_chunk = num - i;
    tx_done = false;
    // we use interupt to be sure the big blob of data we
    // transmit is correctly sent and received by the host
    HAL_UART_Transmit_IT(&huart2, (uint8_t *)(&csis[i]),
                         num_csi_in_chunk * sizeof(csi_t));
    while (!tx_done) {
      __WFI();
    }
    tx_done = false;
  }
}

void physec_telemetry_send_keygen_conf(physec_config *info) {
  uint8_t buf[TM_KEYGEN_CONF_MAX_SIZE] = {0};
  physec_telemetry_packet_t *pkt = (physec_telemetry_packet_t *)buf;
  pkt->magic = UART_PHYSEC_TELEMETRY_MAGIC;
  pkt->type = PHYSEC_TELEMETRY_KG_CONF;
  physec_telemetry_keygen_info_t *keygen_info =
      (physec_telemetry_keygen_info_t *)&(pkt->data);
  memcpy(keygen_info, info, sizeof(physec_config));
  HAL_UART_Transmit(&huart2, (uint8_t *)pkt, physec_telemetry_get_size(pkt),
                    200);
}

void physec_telemetry_send_keygen_info(uint8_t key_type, const uint8_t *key,
                                       size_t num_bits) {
  uint8_t buf[TM_KEYGEN_INFO_MAX_SIZE] = {0};
  physec_telemetry_packet_t *pkt = (physec_telemetry_packet_t *)buf;
  pkt->magic = UART_PHYSEC_TELEMETRY_MAGIC;
  pkt->type = PHYSEC_TELEMETRY_KG_INFO;
  physec_telemetry_keygen_info_t *keygen_info =
      (physec_telemetry_keygen_info_t *)&(pkt->data);
  keygen_info->key_type = key_type;
  keygen_info->num_bits = num_bits;
  size_t num_bytes = (num_bits + 8 - 1) / 8;
  memcpy(keygen_info->key, key, num_bytes);
  HAL_UART_Transmit(&huart2, (uint8_t *)pkt, physec_telemetry_get_size(pkt),
                    1000);
}
