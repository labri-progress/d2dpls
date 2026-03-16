#include "physec_radio.h"
#include "physec_telemetry.h"
#include "radio.h"
#include "subghz_phy_app.h"

#include "libphysec/acquisition.h"
#include "libphysec/packet.h"
#include "libphysec/pre_processing.h"
#include "libphysec/quantization.h"
#include "libphysec/reconciliation/reconciliation.h"
#include "libphysec/types.h"
#include "libphysec/utils.h"
#include "physec_serial.h"
#include "utilities_conf.h"
#include "utilities_def.h"

#include <stdbool.h>
#include <stdint.h>
/*!
 *  Compute LoRa AirTime for a given packet and lora configuration
 *
 *  Be Careful, this function doesn't performs any checks on the arguments
 */
uint32_t lora_time_on_air(uint8_t sf, uint8_t bw, uint8_t preamble_len,
                          uint16_t payload_len, bool crc_on) {
  UNUSED(sf);
  UNUSED(bw);
  UNUSED(preamble_len);
  UNUSED(payload_len);
  UNUSED(crc_on);
  // TODO: use semtech's formula to calulcate airtime
  return 0;
}

/*!
 *	\brief OnRxDone Callback. Called when a packet has been successfully
 *received. This callback is usefull fetch wireless channel characteristics,
 *and store packet payload
 */
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi,
              int8_t LoraSnr_FskCfo) {
  /* USER CODE BEGIN OnRxDone */
  // tm_plog(TS_ON, VLEVEL_L, "OnRxDone2\n\r");
#if ((USE_MODEM_LORA == 1) && (USE_MODEM_FSK == 0))
  // tm_plog(TS_ON, VLEVEL_L, "RssiValue=%d dBm, SnrValue=%ddB\n\r", rssi,
  //         LoraSnr_FskCfo);
  /* Record payload Signal to noise ratio in Lora*/
  SnrValue = LoraSnr_FskCfo;

  // Channel State Information feature retrieving
  switch (physec_conf.keygen.csi_type) {
  case CSI_ADJACENT_REGISTER_RSSI:
  case CSI_REGISTER_RSSI:
    num_reg_rssis = MAX_SYMBOLS_PER_FRAME;
    Radio.PLSGetLastFrameRegRSSIs(reg_rssis, &num_reg_rssis);
    break;
  case CSI_PACKET_RSSI:
    RssiValue = rssi;
    break;
  default:
    break;
  }

#endif /* USE_MODEM_LORA | USE_MODEM_FSK */
#if ((USE_MODEM_LORA == 0) && (USE_MODEM_FSK == 1))
  // tm_plog(TS_ON, VLEVEL_L, "RssiValue=%d dBm, Cfo=%dkHz\n\r", rssi,
  // LoraSnr_FskCfo);
  SnrValue = 0; /*not applicable in GFSK*/
#endif          /* USE_MODEM_LORA | USE_MODEM_FSK */
  /* Update the State of the FSM*/
  State = RX;
  /* Clear BufferRx*/
  memset(BufferRx, 0, MAX_APP_BUFFER_SIZE);
  /* Record payload size*/
  RxBufferSize = size;
  if (RxBufferSize <= MAX_APP_BUFFER_SIZE) {
    memcpy(BufferRx, payload, RxBufferSize);
  }

  /* Record Received Signal Strength*/
  RssiValue = rssi;
  SnrValue = LoraSnr_FskCfo;
  /* Record payload content*/
  // tm_plog(TS_ON, VLEVEL_H, "payload. size=%d \n\r", size);
  // for (int i = 0; i < PAYLOAD_LEN; i++)
  //{
  //   //tm_plog(TS_OFF, VLEVEL_H, "%02X", BufferRx[i]);
  //   if (i % 16 == 15)
  //   {
  //     //tm_plog(TS_OFF, VLEVEL_H, "\n\r");
  //   }
  // }
  // tm_plog(TS_OFF, VLEVEL_H, "\n\r");
  /* Run PingPong process in background*/
  tm_plog(TS_ON, VLEVEL_L, "< received %lu bytes.", size);
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
  /* USER CODE END OnRxDone */
}

/* Private functions
 * ---------------------------------------------------------*/

void OnTxDone(void) {
  /* USER CODE BEGIN OnTxDone */
  // tm_plog(TS_ON, VLEVEL_L, "OnTxDone\n\r");
  /* Update the State of the FSM*/
  State = TX;
  /* Run PingPong process in background*/
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
  /* USER CODE END OnTxDone */
}

void OnTxTimeout(void) {
  /* USER CODE BEGIN OnTxTimeout */
  // tm_plog(TS_ON, VLEVEL_L, "OnTxTimeout\n\r");
  /* Update the State of the FSM*/
  tm_plog(TS_ON, VLEVEL_L, "TxTimeout\n\r");
  State = TX_TIMEOUT;
  /* Run PingPong process in background*/
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
  /* USER CODE END OnTxTimeout */
}

void OnRxTimeout(void) {
  /* USER CODE BEGIN OnRxTimeout */
  // tm_plog(TS_ON, VLEVEL_L, "OnRxTimeout\n\r");
  /* Update the State of the FSM*/
  tm_plog(TS_ON, VLEVEL_L, "RxTimeout\n\r");
  State = RX_TIMEOUT;
  /* Run PingPong process in background*/
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
  /* USER CODE END OnRxTimeout */
}

void OnRxError(void) {
  /* USER CODE BEGIN OnRxError */
  tm_plog(TS_ON, VLEVEL_L, "OnRxError\n\r");
  /* Update the State of the FSM*/
  State = RX_ERROR;
  // TODO: is that a good idea ?
  random_delay = (Radio.Random()) >> 22;
  /* Run PingPong process in background*/
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), CFG_SEQ_Prio_0);
  /* USER CODE END OnRxError */
}

ResetStates_t reset_state = NOT_RSTING;

void send_rst(void) {
  physec_packet_t *packet =
      build_reset_packet(BufferTx, MAX_APP_BUFFER_SIZE, 0);
  Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
  tm_plog(TS_ON, VLEVEL_L, "Sent RST packet");
}

void send_rst_ack(void) {
  physec_packet_t *packet =
      build_reset_packet(BufferTx, MAX_APP_BUFFER_SIZE, 1);
  Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
  tm_plog(TS_ON, VLEVEL_L, "Sent RST ACK packet");
}

/// handle reset state machine transitions
// paste code below at https://www.mermaidchart.com
// flowchart LR
// NOT_RSTING -- USER_BTN --> SEND_RST --> IsAlice{Alice ?}
// NOT_RSTING -- RECV_RST --> IsAlice2{Alice ?}
// IsAlice -- NO --> WAIT_RST
// IsAlice -- YES --> WAIT_ACK
// WAIT_RST -- TIMEOUT --> SEND_RST
// WAIT_RST -- RECV_RST --> SEND_ACK
// WAIT_ACK -- TIMEOUT --> SEND_RST
// WAIT_ACK -- ACK --> NOT_RSTING
// IsAlice2 -- YES --> SEND_RST
// IsAlice2 -- NO --> SEND_ACK
// SEND_ACK --> WAIT_PROBE
// WAIT_PROBE -- TIMEOUT --> SEND_ACK
// WAIT_PROBE -- PROBE --> NOT_RSTING
void reset_handler(int requested_transition) {
  bool is_master = physec_conf.keygen.is_master;
  switch (reset_state) {
  case NOT_RSTING:
    if (requested_transition == KG_RST_REQ ||
        requested_transition == USER_BTN) {
      if (is_master) {
        reset_state = WAIT_ACK;
      } else {
        reset_state = WAIT_RST;
      }
      send_rst();
      reset_physec_states();
      tm_plog(TS_ON, VLEVEL_M, "Performing Reset");
    } else if (requested_transition == RECV_RST) {
      if (is_master) {
        send_rst();
        reset_state = WAIT_ACK;
      } else {
        send_rst_ack();
        reset_state = WAIT_PROBE;
      }
      reset_physec_states();
    }
    break;
  case WAIT_RST:
    if (requested_transition == TIMEOUT ||
        requested_transition == RECV_FIRST_PROBE) {
      send_rst();
    } else if (requested_transition == RECV_RST) {
      send_rst_ack();
      reset_state = WAIT_PROBE;
    }
    break;
  case WAIT_PROBE:
    if (requested_transition == TIMEOUT || requested_transition == RECV_RST) {
      send_rst_ack();
    } else if (requested_transition == RECV_FIRST_PROBE) {
      reset_state = NOT_RSTING;
    }
    break;
  case WAIT_ACK:
    if (requested_transition == TIMEOUT) {
      send_rst();
    } else if (requested_transition == RECV_RST_ACK) {
      reset_state = NOT_RSTING;
    }
    break;
  default:
    break;
  }
}

States_t State = RX;

uint8_t BufferRx[MAX_APP_BUFFER_SIZE];
uint8_t BufferTx[MAX_APP_BUFFER_SIZE];

/* Last  Received Buffer Size*/
uint16_t RxBufferSize = 0;
/* Last  Received packer Rssi*/
int8_t RssiValue = 0;
/* Last  Received packer SNR (in Lora modulation)*/
int8_t SnrValue = 0;

int32_t random_delay;

int32_t time_last_pp_send_all = 0;

int16_t reg_rssis[MAX_SYMBOLS_PER_FRAME] = {0};
size_t num_reg_rssis = 0;
size_t last_added_num_reg_rssis = 0;
RadioEvents_t RadioEvents;

void fe_stl_init() {
  prng_init(&r_prng, "rrng", 5);
  prng_init(&l_prng, "lrng", 5);
  prng_init(&m_prng, "mrng", 5);
  set_log(&logger);
}

void fe_stl_create_and_send_locks() {
  tm_plog(TS_ON, VLEVEL_L, "starting locks creating\r\n");
  uint32_t num_possible_locks =
      (MAX_APP_BUFFER_SIZE - RECON_PACKET_HEADER_SIZE) / FE_STL_LOCK_SIZE;
  tm_plog(TS_ON, VLEVEL_L, "tring to create %d locks (max: %d)\r\n",
          FE_STL_NUM_LOCKS_PER_TRY, num_possible_locks);
  if (num_possible_locks < FE_STL_NUM_LOCKS_PER_TRY) {
    FAIL("Trying to create more locks than possible to send");
  }

  uint8_t ciphers[FE_STL_NUM_LOCKS_PER_TRY][AES_KEY_SIZE_IN_BYTES + FE_STL_SEC];
  uint8_t nonces[FE_STL_NUM_LOCKS_PER_TRY][AES_KEY_SIZE_IN_BYTES];
  uint8_t masks[FE_STL_NUM_LOCKS_PER_TRY][AES_KEY_SIZE_IN_BYTES];

  uint8_t *ciphers_ptrs[FE_STL_NUM_LOCKS_PER_TRY];
  uint8_t *nonces_ptrs[FE_STL_NUM_LOCKS_PER_TRY];
  uint8_t *masks_ptrs[FE_STL_NUM_LOCKS_PER_TRY];

  for (int i = 0; i < FE_STL_NUM_LOCKS_PER_TRY; ++i) {
    ciphers_ptrs[i] = ciphers[i];
    nonces_ptrs[i] = nonces[i];
    masks_ptrs[i] = masks[i];
  }

  uint8_t r_buf[AES_KEY_SIZE_IN_BYTES];
  uint8_t r_p_buf[AES_KEY_SIZE_IN_BYTES];
  uint8_t w_i_buf[AES_KEY_SIZE_IN_BYTES];

  fe_helpers_t helpers;
  helpers.ciphers = ciphers_ptrs;
  helpers.nonces = nonces_ptrs;
  helpers.masks = masks_ptrs;

  fe_gen(physec_key, AES_KEY_SIZE_IN_BYTES, FE_STL_NUM_LOCKS_PER_TRY, FE_STL_K,
         r_buf, AES_KEY_SIZE_IN_BYTES, FE_STL_SEC, AES_KEY_SIZE_IN_BYTES,
         &helpers, w_i_buf, &r_prng, &m_prng, &l_prng);
  physec_packet_t *pkt = build_recon_fe_stl_packet(
      &helpers, BufferTx, MAX_APP_BUFFER_SIZE, AES_KEY_SIZE_IN_BYTES,
      FE_STL_SEC, FE_STL_NUM_LOCKS_PER_TRY);
  recon_num_try += FE_STL_NUM_LOCKS_PER_TRY;
  memcpy(recon_key, r_buf, AES_KEY_SIZE_IN_BYTES);

  Radio.Send((uint8_t *)pkt, physec_packet_get_size(pkt));
}

bool fe_stl_reproduce_received_locks(physec_recon_packet_t *pkt) {
  fe_helpers_t helpers;
  uint8_t ciphers[FE_STL_NUM_LOCKS_PER_TRY][AES_KEY_SIZE_IN_BYTES + FE_STL_SEC];
  uint8_t nonces[FE_STL_NUM_LOCKS_PER_TRY][AES_KEY_SIZE_IN_BYTES];
  uint8_t masks[FE_STL_NUM_LOCKS_PER_TRY][AES_KEY_SIZE_IN_BYTES];

  uint8_t *ciphers_ptrs[FE_STL_NUM_LOCKS_PER_TRY];
  uint8_t *nonces_ptrs[FE_STL_NUM_LOCKS_PER_TRY];
  uint8_t *masks_ptrs[FE_STL_NUM_LOCKS_PER_TRY];

  const uint32_t chunk_size = (AES_KEY_SIZE_IN_BYTES * 3 + FE_STL_SEC);
  for (int i = 0; i < FE_STL_NUM_LOCKS_PER_TRY; ++i) {
    uint32_t offset = chunk_size * i;

    ciphers_ptrs[i] = ciphers[i];
    nonces_ptrs[i] = nonces[i];
    masks_ptrs[i] = masks[i];

    memcpy(ciphers_ptrs[i], pkt->data.helpers + offset,
           AES_KEY_SIZE_IN_BYTES + FE_STL_SEC);

    memcpy(nonces_ptrs[i],
           pkt->data.helpers + offset + (AES_KEY_SIZE_IN_BYTES + FE_STL_SEC),
           AES_KEY_SIZE_IN_BYTES);

    memcpy(masks_ptrs[i],
           pkt->data.helpers + offset + (AES_KEY_SIZE_IN_BYTES + FE_STL_SEC) +
               AES_KEY_SIZE_IN_BYTES,
           AES_KEY_SIZE_IN_BYTES);
  }

  helpers.ciphers = ciphers_ptrs;
  helpers.nonces = nonces_ptrs;
  helpers.masks = masks_ptrs;
  uint8_t r_buf[AES_KEY_SIZE_IN_BYTES];
  uint8_t temp_buf[AES_KEY_SIZE_IN_BYTES];
  uint8_t w_i_buf[AES_KEY_SIZE_IN_BYTES];

  for (uint32_t l_idx = 0; l_idx < FE_STL_NUM_LOCKS_PER_TRY; l_idx++) {
    if (fe_rep(physec_key, AES_KEY_SIZE_IN_BYTES, FE_STL_NUM_LOCKS_PER_TRY,
               FE_STL_SEC, r_buf, AES_KEY_SIZE_IN_BYTES, AES_KEY_SIZE_IN_BYTES,
               w_i_buf, &helpers, temp_buf, &l_prng)) {
      memcpy(recon_key, r_buf, AES_KEY_SIZE_IN_BYTES);
      return true;
    }
  }
  return false;
}
