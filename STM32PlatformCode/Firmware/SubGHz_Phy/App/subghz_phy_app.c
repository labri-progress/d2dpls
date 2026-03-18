/*!
 * \file      subghz_phy_app.c
 *
 * \brief     PHYKeyGen implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
/**
 ******************************************************************************
 *
 *          Portions COPYRIGHT 2020 STMicroelectronics
 *
 * @file    subghz_phy_app.c
 * @author  MCD Application Team
 * @brief   Application of the SubGHz_Phy Middleware
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "subghz_phy_app.h"

#include "app_version.h"
#include "cmox_crypto.h"
#include "hash/cmox_sha256.h"
#include "physec_config.h"
#include "physec_radio.h"
#include "physec_telemetry.h"
#include "physec_utils.h"
#include "platform.h"
#include "stm32l0xx_hal.h"
#include "stm32l0xx_hal_rng.h"
#include "stm32l0xx_hal_uart.h"
#include "sx1276.h"
#include "sys_app.h"
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "dma.h"
#include "radio.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "usart.h"
#include "usart_if.h"
#include "utilities_conf.h"
#include "utilities_def.h"

// #include "lorawan_aes.h"
// #include "cmac.h"

#define __PHYSEC_SX1276__
#include "libphysec/acquisition.h"
#include "libphysec/packet.h"
#include "libphysec/pre_processing.h"
#include "libphysec/quantization.h"
#include "libphysec/reconciliation/reconciliation.h"
#include "libphysec/types.h"
#include "libphysec/utils.h"
#include "physec_serial.h"

/**
 * PHYsec
 *
 * Key Generation:
 *	- Alice is the master, and Bob the slave
 *	- Alice calculates the metrics on datasets and results (keys)
 *;
 *
 */

/* USER CODE END Includes */

/* External variables
 * ---------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Led Timers objects*/
UTIL_TIMER_Object_t timerLed;
uint8_t uart_rx_buf[MAX_UART_BUF_SIZE] = {0};

/* Related to key generation phases */

size_t num_quantized_csi = 0;
size_t num_csi = 0;
size_t num_indexes = 0;
size_t probe_cnt = 0;
size_t num_remote_indexes = 0;

uint8_t physec_key[KEY_CAPACITY_IN_BYTES] = {0};
int16_t physec_key_num_bits = -1;
csi_t csi_measures[NUM_MAX_CSI] = {0};
quant_index_t indexes[NUM_MAX_QUANT_INDEX] = {0};
// used for excursion based quant where you need
// to receive all indexes before computing final
// indexes set (intersection of excursions from Alice and Bob)
quant_index_t remote_indexes[NUM_MAX_EXCURSIONS(QUANT_SB_EXCURSION_MAX_M)] = {
    0};
quant_lossy_params_t quant_params = {0};
uint16_t num_csi_more = 0;
// the number of slave unsucessfull tries to quantize,
// it helps determining how many probes we will
// exchange before trying quantization again
uint8_t num_quant_try = 0;
bool post_process_expired = false;
// MAX_LOSSY_CHUNKS = 16, fits in an uint16_t
uint8_t post_process_num_chunks_to_rx = 0;
lossy_chunk_bitmap_t post_process_rx_chunks = 0;

// TODO: better state managements than lost alone booleans
bool on_config = false;
bool config_done = false;

uint8_t quant_status = QUANT_STATUS_WAITING;
int physec_state = PHYSEC_STATE_PROBING;
extern physec_config physec_conf;

/* Reconciliation (Work In Progress)*/
uint32_t recon_num_try = 0;
uint8_t recon_key[AES_KEY_SIZE_IN_BYTES];
physec_prng_t r_prng;
physec_prng_t l_prng;
physec_prng_t m_prng;

void logger(const char *fmt, va_list args) {
  char buf[1024];
  vsnprintf(buf, sizeof(buf), fmt, args);
  tm_plog(TS_ON, VLEVEL_M, "%s\r\n", buf);
}

/* UART related */
uint8_t uart_buf[256] = {0};
size_t uart_bufsize = 0;
uint8_t cfg_type = 0xff;
size_t expected = 0;
size_t num_expected_csi = 0;
uint32_t csi_packet_bytes_remaining = 0;

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/**
 * @brief  Function executed on when led timer elapses
 * @param  context ptr of LED context
 */
static void OnledEvent(void *context);

/**
 * @brief PHYsec_Platform state machine implementation
 */

/* USER CODE END PFP */
/* Exported functions
 * ---------------------------------------------------------*/
void SubghzApp_Init(void) {
  /* USER CODE BEGIN SubghzApp_Init_1 */
  /* Print APP version*/
  tm_plog(TS_OFF, VLEVEL_M, "APP_VERSION= V%X.%X.%X\r\n",
          (uint8_t)(__APP_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__APP_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__APP_VERSION >> __APP_VERSION_SUB2_SHIFT));

  /* Led Timers*/
  UTIL_TIMER_Create(&timerLed, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnledEvent,
                    NULL);
  UTIL_TIMER_SetPeriod(&timerLed, LED_PERIOD_MS);
  UTIL_TIMER_Start(&timerLed);
  /* USER CODE END SubghzApp_Init_1 */

  /* Radio initialization */
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;

  Radio.Init(&RadioEvents);

  /*calculate random delay for synchronization*/
  random_delay =
      (Radio.Random()) >> 22; /*10bits random e.g. from 0 to 1023 ms*/

  /* USER CODE BEGIN SubghzApp_Init_2 */
  /* Radio Set frequency */
  Radio.SetChannel(RF_FREQUENCY);

  /* Button initialisation for interuption*/
  USER_BUTTON_GPIO_CLK_ENABLE();
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* LED initialization*/
  LED_Init(LED_RED1);
  LED_Init(LED_RED2);
  LED_Init(LED_BLUE);
  LED_On(LED_BLUE);
  /*fills tx buffer*/
  memset(BufferTx, 0x0, MAX_APP_BUFFER_SIZE);

  // tm_plog(TS_ON, VLEVEL_L, "rand=%id\n\r", random_delay);

  // EEPROM read config
  physec_config tmp_conf = {0};
  bool has_config = physec_config_read_eeprom(0, &tmp_conf);
  if (has_config) {
    memcpy(&physec_conf, &tmp_conf, sizeof(physec_config));
  }

  // for testing quantization without probing
  LED_Off(LED_RED1);
  LED_Off(LED_RED2);
  HAL_UART_SetRxConfig(&huart2, HAL_UART_RxCpltCallback, uart_rx_buf, 1);
  wait_physec_config();

  LED_On(LED_RED1);
  LED_On(LED_RED2);

  // EEPROM save new config
  /*  if (!has_config || memcmp(&conf, &physec_conf, sizeof(physec_config)) !=
    0) { eeprom_write_physec_config(&physec_conf);
    }*/
  if (config_done) {
    if (physec_config_write_eeprom(0, &physec_conf) == HAL_OK) {
      tm_plog(TS_OFF, VLEVEL_M,
              "New PHYsec configuration saved in EEPROM !\r\n");
    }
  }

  /* Radio configuration */
#if ((USE_MODEM_LORA == 1) && (USE_MODEM_FSK == 0))
  // tm_plog(TS_OFF, VLEVEL_M, "---------------\n\r");
  // tm_plog(TS_OFF, VLEVEL_M, "LORA_MODULATION\n\r");
  // tm_plog(TS_OFF, VLEVEL_M, "LORA_BW=%d kHz\n\r", (1 << LORA_BANDWIDTH) *
  // 125); tm_plog(TS_OFF, VLEVEL_M, "LORA_SF=%d\n\r", LORA_SPREADING_FACTOR);

  /* Enable Physical Layer Security features of SX1276 driver */
  Radio.SetPLS(true);
  // tm_plog(TS_OFF, VLEVEL_M, "PLS enabled\n\r");

  Radio.SetTxConfig(MODEM_LORA, physec_conf.physical_layer.lora.power, 0,
                    physec_conf.physical_layer.lora.bw,
                    physec_conf.physical_layer.lora.sf, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON, true, 0,
                    0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);

  Radio.SetRxConfig(MODEM_LORA, physec_conf.physical_layer.lora.bw,
                    physec_conf.physical_layer.lora.sf, LORA_CODINGRATE, 0,
                    LORA_PREAMBLE_LENGTH, LORA_SYMBOL_TIMEOUT,
                    LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
                    LORA_IQ_INVERSION_ON, true);

  Radio.SetMaxPayloadLength(MODEM_LORA, MAX_APP_BUFFER_SIZE);

#else
#error "Please define a modulation in the subghz_phy_app.h file."
#endif /* USE_MODEM_LORA | USE_MODEM_FSK */

  // log current config
  tm_plog(TS_OFF, VLEVEL_M, "PHYsec configuration window elapsed !\r\n");
  tm_send_keygen_conf();
  tm_plog(TS_OFF, VLEVEL_M, "Role = %s\n\r",
          (physec_conf.keygen.is_master) ? "Master" : "Slave");
  tm_plog(TS_OFF, VLEVEL_M, "DEBUG: CSI Type = %d\n\r",
          physec_conf.keygen.csi_type);
  tm_plog(TS_OFF, VLEVEL_M, "DEBUG: Quant Type = %d\n\r",
          physec_conf.keygen.quant_type);
  tm_plog(TS_OFF, VLEVEL_M, "DEBUG: Reconciliation Type = %d\n\r",
          physec_conf.keygen.recon_type);
  tm_plog(TS_OFF, VLEVEL_M, "Radio Conf:\n\r");
  tm_plog(TS_OFF, VLEVEL_M, "Modulation = %s\n\r",
          (physec_conf.physical_layer.modulation == PHYSEC_PHY_MODULATION_LORA)
              ? "LORA"
              : "FSK");
  if (physec_conf.physical_layer.modulation == PHYSEC_PHY_MODULATION_LORA) {
    tm_plog(TS_OFF, VLEVEL_M, "- SF = %d\n\r",
            physec_conf.physical_layer.lora.sf);
    tm_plog(TS_OFF, VLEVEL_M, "- BW = %d kHz\n\r",
            (1 << physec_conf.physical_layer.lora.bw) * 125);
    tm_plog(TS_OFF, VLEVEL_M, "- Power = %d dBm\n\r",
            physec_conf.physical_layer.lora.power);
  }

  if (num_csi > 0) {
    // getting to the right number of CSI, as if they were obtained through
    // probing.
    if (physec_conf.keygen.quant_type == QUANT_MBR_LOSSLESS) {
      // first of all we check if it is even possible to quantize
      if (measurements_sufficiency_check_mbr_lossless(csi_measures, num_csi)) {
        // then we find the minimum amount of CSI we need
        bool found_min_num_csi = false;
        for (int required_csi = 0; required_csi < num_csi && !found_min_num_csi;
             required_csi++) {
          if (measurements_sufficiency_check_mbr_lossless(csi_measures,
                                                          required_csi)) {
            // we found the minimum number of CSI required
            num_csi = required_csi;
            found_min_num_csi = true;
          }
        }
      }
    }
    probe_cnt = num_csi;
    tm_plog(TS_ON, VLEVEL_M, "CSI INIT:\r\n");
    tm_send_csis(csi_measures, num_csi);
    if (physec_conf.keygen.is_master &&
        pre_process_and_quantize() >= REQUIRED_NUM_BITS_PER_KEY) {
      tm_plog(TS_ON, VLEVEL_M, "> Keygen Done ! (%d bits)\n\r",
              physec_key_num_bits);
      tm_send_key_info(KEY_TYPE_QUANT, physec_key, physec_key_num_bits);
      tm_plog(TS_ON, VLEVEL_L, "Alice's Preliminary Key = [ ");
      for (size_t i = 0; i < AES_KEY_SIZE_IN_BYTES; i++) {
        if (i == AES_KEY_SIZE_IN_BYTES - 1) {
          tm_plog(TS_OFF, VLEVEL_L, "%02x ]\n\r", physec_key[i]);
        } else {
          tm_plog(TS_OFF, VLEVEL_L, "%02x,", physec_key[i]);
        }
      }

      if (QUANT_IS_LOSSY(physec_conf.keygen.quant_type)) {
        physec_state = PHYSEC_STATE_POST_KEYGEN_SEND;
      } else {
        physec_state = PHYSEC_STATE_KEYGEN;
      }
    }
  }

  /*starts reception*/
  Radio.Rx(RX_TIMEOUT_VALUE + random_delay);
  /*register task to to be run in while(1) after Radio IT*/
  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), UTIL_SEQ_RFU,
                   PHYsec_Platform_Process);
  cmox_init(); // init cmox STM32 module
  fe_stl_init();

  /* USER CODE END SubghzApp_Init_2 */
}

/* USER CODE BEGIN EF */

/*!
 *	\brief	Wait for a specific period of time for PHYsec configuration
 *
 *	Abort on timeout expiration or configuration done. Configuration data
 *	is handled and parsed by the UART IRQ.
 */
static void wait_physec_config(void) {
  uint32_t start_time = HAL_GetTick();
  while (on_config || HAL_GetTick() - start_time < UART_CONFIG_TIME_WINDOW_MS) {
    __WFI();
    // HAL_Delay(100);
    if (config_done)
      break;
  }
}

/*!
 *	\brief Post process sending of quantization indexes
 *
 *	\param[in] to_send		bitmap of indexes to send
 *
 */
static void post_process_send_indexes(lossy_chunk_bitmap_t to_send) {
  tm_plog(TS_ON, VLEVEL_L, "> PLS - Total indexes: %u\n\r", num_indexes);
  tm_plog(TS_ON, VLEVEL_L, "> PLS - Sending 0x%x\n\r", to_send);
  size_t num_chunks =
      (num_indexes + MAX_LOSSY_PER_RADIO_FRAME - 1) / MAX_LOSSY_PER_RADIO_FRAME;
  if (num_chunks > MAX_LOSSY_CHUNKS)
    num_chunks = MAX_LOSSY_CHUNKS;

  for (size_t i = 0; i < num_chunks; i++) {
    size_t num_indexes_to_send = 0;
    if ((to_send >> i) & 0x1) {
      tm_plog(TS_ON, VLEVEL_L, "> PLS - Sending lossy indexes chunk %u\n\r", i);
      size_t num_indexes_chunk =
          (i == num_chunks - 1) ? (num_indexes - i * MAX_LOSSY_PER_RADIO_FRAME)
                                : MAX_LOSSY_PER_RADIO_FRAME;
      quant_index_t *indexes_chunk = indexes + i * MAX_LOSSY_PER_RADIO_FRAME;
      physec_packet_t *packet =
          build_keygen_data_packet(i, indexes_chunk, num_indexes_chunk,
                                   num_indexes, BufferTx, MAX_APP_BUFFER_SIZE);

      Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));

      uint32_t toa = lora_toa(physec_conf.physical_layer.lora.sf,
                              physec_conf.physical_layer.lora.bw,
                              physec_packet_get_size(packet));
      HAL_Delay(Radio.GetWakeupTime() + toa);
    }
  }
}

/*!
 *	\brief Post process receiving of quantization indexes
 *
 *	Merge received indexes with local indexes array in case of Bob. Just
 *copy indexes in case of Alice. This also retrieve the expected number of
 *quantization indexes chunks and updates the received indexes bitmap
 *
 *	\param[in] indexes_pkt		Received indexes packet
 *	\param[in] master			PHYsec Role
 *
 *	\return -1 in case of error (no enough memory, ...), 0 if there is still
 *	chunks to receive, or the total number of chunks to receive if all
 *chunks have been received.
 */
static int post_process_handle_rx_indexes(physec_keygen_data_t *indexes_pkt,
                                          bool master) {
  UNUSED(master);
  if (num_indexes + indexes_pkt->num_dropped > NUM_MAX_QUANT_INDEX) {
    // TODO: truncate or abort
    return -1;
  }
  if (indexes_pkt->dropped_chunk_id >= MAX_LOSSY_CHUNKS) {
    return -1;
  }

  if (indexes_pkt->dropped_chunk_id == 0) {
    post_process_num_chunks_to_rx =
        (MAX_LOSSY_PER_RADIO_FRAME + indexes_pkt->num_dropped - 1) /
        MAX_LOSSY_PER_RADIO_FRAME;
  } else if (post_process_num_chunks_to_rx == 0) {
    // 1st lossy vec not received, deduce total size
    post_process_num_chunks_to_rx =
        indexes_pkt->dropped_chunk_id +
        (MAX_LOSSY_PER_RADIO_FRAME + indexes_pkt->num_dropped - 1) /
            MAX_LOSSY_PER_RADIO_FRAME;
  }
  tm_plog(TS_ON, VLEVEL_L,
          "> PLS - Total lossy indexes to Rx: %u (expected %u chunks)\n\r",
          indexes_pkt->num_dropped, post_process_num_chunks_to_rx);
  tm_plog(TS_ON, VLEVEL_L, "Local num indexes = %u\n\r", num_indexes);

  if ((1 << indexes_pkt->dropped_chunk_id) & post_process_rx_chunks) {
    // already received
    return 0;
  }
  size_t to_copy = indexes_pkt->num_dropped > MAX_LOSSY_PER_RADIO_FRAME
                       ? MAX_LOSSY_PER_RADIO_FRAME
                       : indexes_pkt->num_dropped;

  tm_plog(TS_ON, VLEVEL_L, "num_indexes = %u\n\r", num_indexes);
  for (size_t i = 0; i < to_copy; i++) {
    tm_plog(TS_ON, VLEVEL_L, "index[%d] = %u\n\r",
            indexes_pkt->dropped_chunk_id * MAX_LOSSY_PER_RADIO_FRAME + i,
            indexes_pkt->dropped[i]);
  }
  switch (physec_conf.keygen.quant_type) {
  case QUANT_SB_EXCURSION_LOSSY:
  case QUANT_MB_EXCURSION_LOSSY: {
    // cannot perform intersection
    // without receiving whole
    // indexes from remote host
    quant_index_t *cur_chunk_remote_idxs_start =
        remote_indexes +
        indexes_pkt->dropped_chunk_id * MAX_LOSSY_PER_RADIO_FRAME;
    memcpy(cur_chunk_remote_idxs_start, indexes_pkt->dropped,
           to_copy * sizeof(quant_index_t));
    num_remote_indexes += to_copy;
    break;
  }
  default:
    if (!quant_merge_csi_indexes(indexes, &num_indexes, NUM_MAX_QUANT_INDEX,
                                 indexes_pkt->dropped, to_copy)) {
      // TODO: abort
      return -1;
    }

    break;
  }

  post_process_rx_chunks |=
      (1 << indexes_pkt->dropped_chunk_id); // add chunk id to received bitmap
  tm_plog(TS_ON, VLEVEL_L,
          "> PLS - Received lossy indexes chunk %u (num=%u)\n\r",
          indexes_pkt->dropped_chunk_id, indexes_pkt->num_dropped);

  if (bitmap_hamming_weight(post_process_rx_chunks) ==
      post_process_num_chunks_to_rx) {
    return post_process_num_chunks_to_rx;
  } else if (post_process_expired) {
    // TODO: move out to rx timeout
  }
  return 0;
}

static int handle_keygen(physec_keygen_packet_t *kg_pkt, bool master) {
  if (!master && quant_status != QUANT_STATUS_DONE) {
    // first keygen packet indicate slave to quantize
    int quant_ret = pre_process_and_quantize();
    tm_plog(TS_ON, VLEVEL_L, "> Slave Q() (%d measures) => %d bits\n\r",
            num_csi, quant_ret);
    if (quant_ret < REQUIRED_NUM_BITS_PER_KEY) {
      quant_status = QUANT_STATUS_FAILURE;
      return KG_QUANT_ERROR;
    }
    tm_plog(TS_ON, VLEVEL_L, "CSI handle keygen:\r\n");
    tm_send_csis(csi_measures, num_csi);
    tm_plog(TS_ON, VLEVEL_M, "> Keygen Done ! (%d bits)\n\r",
            physec_key_num_bits);
    tm_send_key_info(KEY_TYPE_QUANT, physec_key, physec_key_num_bits);
    // physec_key_num_bits = quant_ret;
    quant_status = QUANT_STATUS_DONE;
  }

  switch (kg_pkt->kg_type) {
  case PHYSEC_KEYGEN_TYPE_DONE: {
    return KG_SLAVE_DONE;
    break;
  }
  case PHYSEC_KEYGEN_TYPE_DATA: {
    physec_keygen_data_t *kg_data = (physec_keygen_data_t *)&(kg_pkt->data);
    if (!QUANT_IS_LOSSY(physec_conf.keygen.quant_type)) {
      if (!master && kg_data->num_dropped == 0 &&
          kg_data->dropped_chunk_id == 0) {
        // Master successfully quantize
        return KG_DONE;
      }
      // invalid packet for lossless quantization
      break;
    }

    if (post_process_handle_rx_indexes(kg_data, physec_conf.keygen.is_master) <=
        0) {
      return KG_PP_WAIT_MORE;
    }

    if (master) {
      return KG_DONE;
    } else {
      size_t num_information_bits = 0;
      if (QUANT_IS_EXCURSION(physec_conf.keygen.quant_type)) {
        num_information_bits = num_remote_indexes;
      } else {
        num_information_bits = physec_key_num_bits - num_indexes;
      }
      if (num_information_bits >= REQUIRED_NUM_BITS_PER_KEY) {
        // enough bits after post processing
        return KG_PP_SEND_ALL;
      } else {
        // too much points dropped, not enough bits
        // anymore
        quant_status = QUANT_STATUS_FAILURE;
        return KG_QUANT_ERROR;
      }
    }
  }
  case PHYSEC_KEYGEN_TYPE_RETRANSMISSION_REQ: {
    physec_keygen_retransmission_req_t *retransmission_req =
        (physec_keygen_retransmission_req_t *)&(kg_pkt->data);

    if (!master &&
        (physec_state >= PHYSEC_STATE_RECONCILIATION ||
         bitmap_hamming_weight(retransmission_req->lost_chunks_bitmap) == 0)) {
      return KG_DONE;
    }

    return KG_PP_SEND_LOST;
  }
  case PHYSEC_KEYGEN_TYPE_ERROR:
    return KG_QUANT_ERROR;
  default:
    break;
  }

  return KG_QUANT_ERROR;
}

// Performs quantization (Master only function)
static bool do_keygen(void) {

  int quant_ret = pre_process_and_quantize();
  tm_plog(TS_ON, VLEVEL_L,
          "nb of bits in the physec_key returned by pre_process_and_quantize = "
          "%d bits:\r\n",
          quant_ret);
  if (quant_ret < REQUIRED_NUM_BITS_PER_KEY) {
    return false;
  }
  tm_plog(TS_ON, VLEVEL_L, "CSI do keygen:\r\n");

  tm_send_csis(csi_measures, num_csi);
  tm_send_key_info(KEY_TYPE_QUANT, physec_key, physec_key_num_bits);
  // physec_key_num_bits = quant_ret;

  if (QUANT_IS_LOSSY(physec_conf.keygen.quant_type)) {
    num_indexes =
        (num_indexes > NUM_MAX_QUANT_INDEX) ? NUM_MAX_QUANT_INDEX : num_indexes;
    post_process_send_indexes(ALL_ONES_MASK);
    time_last_pp_send_all = HAL_GetTick();
    physec_state = PHYSEC_STATE_POST_KEYGEN_SEND;
  } else {
    physec_packet_t *packet =
        build_keygen_success_packet_lossless(BufferTx, MAX_APP_BUFFER_SIZE);
    Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
    physec_state = PHYSEC_STATE_KEYGEN;
  }

  return true;
}

static void do_post_processing(void) {
  // display indexes
  uint8_t buf[256] = {0};
  tm_plog(TS_ON, VLEVEL_L, "Indexes:\n\r");
  for (size_t i = 0; i < num_indexes; i += 16) {
    memset(buf, 0, sizeof buf);
    for (size_t j = 0; j < 16; j++) {
      snprintf((char *)buf + j * 4, sizeof(buf) - j * 4, "%03ld ",
               indexes[i + j]);
    }
    tm_plog(TS_ON, VLEVEL_L, "%s\n\r", buf);
  }

  switch (physec_conf.keygen.quant_type) {
  case QUANT_SB_EXCURSION_LOSSY:
    quant_params.dynamic = false;
    quant_params.nbits_per_sample = 1;
  case QUANT_MB_EXCURSION_LOSSY:
    tm_plog(TS_ON, VLEVEL_L, "nbits per sample: %d\n\r",
            quant_params.nbits_per_sample);
    if (num_indexes > 0) {
      quant_inter_csi_indexes(indexes, &num_indexes, NUM_MAX_QUANT_INDEX,
                              remote_indexes, num_remote_indexes);
      tm_plog(TS_ON, VLEVEL_L, "num kept excursions = %d\n\r", num_indexes);
      num_remote_indexes = 0;
      physec_key_num_bits = quant_retain_csis(
          &quant_params, physec_key, physec_key_num_bits, indexes, num_indexes);
      // num_indexes = 0;
    }

    break;
  case QUANT_SB_DIFF_LOSSY:
  case QUANT_SB_LOSSY:
  case QUANT_SB_LOSSY_BLOCKWISE:
    quant_params.dynamic = false;
    quant_params.nbits_per_sample = 1;
    physec_key_num_bits = quant_dropp_csis(
        &quant_params, physec_key, physec_key_num_bits, indexes, num_indexes);
    // num_indexes = 0;
    break;
  case QUANT_ADAPTIVE:
    break;
  default:
    break;
  }
}

/*!
 *	\brief Handle Probe packet reception.
 *
 *	For Master:
 *	- check probe counter
 *	For Slave:
 *	- check probe counter (eventually rollback to previous count
 *	if one get missed, and discard saved CSIs)
 *	For Both:
 *	- handle CSI measures according to CSI type
 *
 *	\param[in] probe_resp		Received probe packet
 *	\param[in] is_master		PHYsec Role
 *
 *	\return 1 if probe is valid, 0 otherwise
 */
int handle_probe(physec_probe_packet_t *probe_resp, bool is_master) {
  uint32_t probe_resp_cnt = probe_resp->cnt;

  if (is_master) {
    if (probe_resp_cnt != probe_cnt) {
      tm_plog(TS_ON, VLEVEL_L, "Unrecoverable probe error");
      return 0;
    }
  } else {
    if (probe_resp_cnt == probe_cnt - 1) {
      probe_cnt--;
      num_csi -= last_added_num_reg_rssis;
    } else if (probe_resp_cnt != probe_cnt) {
      tm_plog(TS_ON, VLEVEL_L, "Unrecoverable probe error");
      return 0;
    }
  }

  size_t kept_rssi = 0;
  switch (physec_conf.keygen.csi_type) {
  case CSI_ADJACENT_REGISTER_RSSI:
    kept_rssi =
        (num_reg_rssis > NUM_KEPT_AR_RSSI) ? NUM_KEPT_AR_RSSI : num_reg_rssis;
    if (physec_conf.keygen.is_master) {
      memcpy(csi_measures + num_csi, reg_rssis + num_reg_rssis - kept_rssi,
             kept_rssi * sizeof(csi_t));
    } else {
      memcpy(csi_measures + num_csi, reg_rssis, kept_rssi * sizeof(csi_t));
    }
    for (size_t i = num_csi; i < num_csi + kept_rssi; i++) {
      csi_measures[i] = normalize_csi(csi_measures[i]);
    }
    num_csi += kept_rssi;
    last_added_num_reg_rssis = kept_rssi;
    break;
  case CSI_REGISTER_RSSI:
    kept_rssi = (num_reg_rssis > NUM_MAX_CSI) ? NUM_MAX_CSI - num_reg_rssis
                                              : num_reg_rssis;
    memcpy(csi_measures + num_csi, reg_rssis, num_reg_rssis * sizeof(csi_t));
    for (size_t i = num_csi; i < num_csi + kept_rssi; i++) {
      csi_measures[i] = normalize_csi(csi_measures[i]);
    }
    num_csi += kept_rssi;
    last_added_num_reg_rssis = kept_rssi;
    break;
  case CSI_PACKET_RSSI:
    csi_measures[num_csi++] = (-1) * normalize_csi(RssiValue);
    tm_plog(TS_ON, VLEVEL_L, "> RssiValue raw: %d\n",
            ((-1) * normalize_csi(RssiValue)));
    last_added_num_reg_rssis = 1;
    break;
  case CSI_CLSSI:
  default:
    break;
  }
  return 1;
}
void send_probe(uint32_t counter) {
  physec_packet_t *packet = build_probe_packet(
      counter, physec_conf.keygen.probe_padding, BufferTx, MAX_APP_BUFFER_SIZE);

  if (physec_conf.keygen.is_master)
    HAL_Delay(physec_conf.keygen.probe_delay);

  Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
  memset(BufferTx, 0, MAX_APP_BUFFER_SIZE);
}

bool physec_validate_packet(uint8_t *packet, size_t size) {
  if (size < sizeof(physec_packet_t)) {
    return false;
  }

  size -= sizeof(physec_packet_t);

  physec_packet_t *pkt = (physec_packet_t *)packet;
  switch (pkt->type) {
  case PHYSEC_PACKET_TYPE_PROBE:
    if (size < sizeof(physec_probe_packet_t)) {
      return false;
    }
    // we have no need of checking padding bytes
    // other than for being sure packet was not corrupted
    // but its not the actual purpose
    // physec_probe_packet_t *probe_pkt = (physec_probe_packet_t
    // *)&(pkt->data); uint8_t padding_size = size -
    // sizeof(physec_probe_packet_t); if (padding_size > 0) {
    //  if (!physec_check_padding_bytes(probe_pkt->padding, padding_size))
    //    return false;
    //}
    break;
  case PHYSEC_PACKET_TYPE_KEYGEN:
    if (size < sizeof(physec_keygen_packet_t)) {
      return false;
    }

    physec_keygen_packet_t *kg_pkt = (physec_keygen_packet_t *)&(pkt->data);
    switch (kg_pkt->kg_type) {
    case PHYSEC_KEYGEN_TYPE_DATA:
      if (size < PHYSEC_PACKET_KEYGEN_DATA_HEADER_SIZE) {
        return false;
      }
      size -= PHYSEC_PACKET_KEYGEN_DATA_HEADER_SIZE;
      physec_keygen_data_t *kg_data = (physec_keygen_data_t *)&(kg_pkt->data);
      if (kg_data->dropped_chunk_id >= MAX_LOSSY_CHUNKS)
        return false;

      break;
    case PHYSEC_KEYGEN_TYPE_RETRANSMISSION_REQ:
      if (size < sizeof(physec_keygen_retransmission_req_t)) {
        return false;
      }
      break;
    case PHYSEC_KEYGEN_TYPE_ERROR:
      break;
    }

    break;
  case PHYSEC_PACKET_TYPE_RECONCILIATION:
    if (size < sizeof(physec_recon_packet_t)) {
      return false;
    }

    physec_recon_packet_t *recon_pkt = (physec_recon_packet_t *)&(pkt->data);
    if (size < recon_pkt->rec_vec_size) {
      return false;
    }

    break;
  case PHYSEC_PACKET_TYPE_ENCRYPTED:
    if (size < sizeof(physec_encrypted_packet_t)) {
      return false;
    }

    physec_encrypted_packet_t *enc_pkt =
        (physec_encrypted_packet_t *)&(pkt->data);
    if (size < enc_pkt->size) {
      return false;
    }

    break;
  case PHYSEC_PACKET_TYPE_RECONCILIATION_RESULT:
    if (size < sizeof(physec_recon_result_packet_t)) {
      return false;
    }
    return true;
    break;
  case PHYSEC_PACKET_TYPE_RESET:
    return true;
  default:
    return false;
  }

  return true;
}

/************	Main program Loop ************/
static void PHYsec_Platform_Process(void) {
  Radio.Sleep();

  // let's keep this switch simple.
  // once it will work, we will try to integrate it better
  if (reset_state > 0) {
    switch (State) {
    case RX:
      tm_plog(TS_ON, VLEVEL_M, "RX");
      if (physec_validate_packet(BufferRx, RxBufferSize)) {
        physec_packet_t *packet = (physec_packet_t *)BufferRx;
        switch (packet->type) {
        case PHYSEC_PACKET_TYPE_RESET:
          physec_reset_packet_t *rst_pck =
              (physec_reset_packet_t *)&packet->data;
          if (rst_pck->ack) {
            tm_plog(TS_ON, VLEVEL_M, "Received RESET ACK");
            reset_handler(RECV_RST_ACK);
          } else {
            tm_plog(TS_ON, VLEVEL_M, "Received RESET");
            reset_handler(RECV_RST);
          }
          break;
        case PHYSEC_PACKET_TYPE_PROBE:
          // TODO: vérifier count à 1
          tm_plog(TS_ON, VLEVEL_M, "Received PROBE");
          reset_handler(RECV_FIRST_PROBE);
          goto keep_going;
          break;
        default:
          tm_plog(TS_OFF, VLEVEL_M, "RECEIVED %d PKT MGK\r\n", packet->type);
          break;
        }
      }
      break;
    case TX:
      break;
    case RX_ERROR:
      tm_plog(TS_ON, VLEVEL_M, "ERR\r\n");
    case RX_TIMEOUT:
    case TX_TIMEOUT:
      reset_handler(TIMEOUT);
      break;
    }
    Radio.Rx(RX_TIMEOUT_VALUE + random_delay);
    return;
  }

keep_going:
  switch (State) {
  case RX:
    int type = ((physec_packet_t *)BufferRx)->type;

    if (physec_validate_packet(BufferRx, RxBufferSize)) {
      tm_plog(TS_ON, VLEVEL_H, "=== PHYsec packet received ===\n\r");
      physec_packet_t *rx_packet = (physec_packet_t *)BufferRx;

      switch (rx_packet->type) {
      case PHYSEC_PACKET_TYPE_PROBE: {
        physec_probe_packet_t *probe_resp =
            (physec_probe_packet_t *)&(rx_packet->data);

        tm_plog(TS_ON, VLEVEL_M,
                "< Probe Received ! (Packet cnt=%u, Internal cnt=%u)\n\r",
                probe_resp->cnt, probe_cnt);

        if (!handle_probe(probe_resp, physec_conf.keygen.is_master)) {
          tm_plog(TS_ON, VLEVEL_M, "Failed to handle probe");
          goto go_to_rx;
        }

        if (quant_status == QUANT_STATUS_FAILURE)
          quant_status = QUANT_STATUS_WAITING;
        physec_state = PHYSEC_STATE_PROBING;
        // Instead of using the type of the quant to do the verification we need
        // to add a more general verifcation like if multi_bits_quant
        if (physec_conf.keygen.is_master) {
          probe_cnt++;
          csi_t *rssi_measures = csi_measures;
          if (num_csi_more > 0) {
            num_csi_more--;
          } else if ((physec_conf.keygen.quant_type == QUANT_MBR_LOSSLESS) &&
                     measurements_sufficiency_check_mbr_lossless(rssi_measures,
                                                                 num_csi)) {
            if (do_keygen()) {
              tm_plog(TS_ON, VLEVEL_M, "> Keygen Done ! (%d bits)\n\r",
                      physec_key_num_bits);
              tm_plog(TS_ON, VLEVEL_H, "[*] Alice's Key = [\n\r");
              for (size_t i = 0; i < AES_KEY_SIZE_IN_BYTES; i++) {
                if (i == AES_KEY_SIZE_IN_BYTES - 1) {
                  tm_plog(TS_OFF, VLEVEL_L, "%02x ]\n\r", physec_key[i]);
                } else {
                  tm_plog(TS_OFF, VLEVEL_L, "%02x,\n\r", physec_key[i]);
                }
              }

              goto go_to_rx;
            } else {
              if (num_csi >= NUM_MAX_CSI) {
                reset_handler(KG_RST_REQ);
              }
            }
          } else if ((physec_conf.keygen.quant_type == QUANT_SB_LOSSLESS) &&
                     (num_csi >= 128)) {
            if (do_keygen()) {
              tm_plog(TS_ON, VLEVEL_M, "> Keygen Done ! (%d bits)\n\r",
                      physec_key_num_bits);
              tm_plog(TS_ON, VLEVEL_H, "[*] Alice's Key = [\n\r");
              for (size_t i = 0; i < AES_KEY_SIZE_IN_BYTES; i++) {
                if (i == AES_KEY_SIZE_IN_BYTES - 1) {
                  tm_plog(TS_OFF, VLEVEL_L, "%02x ]\n\r", physec_key[i]);
                } else {
                  tm_plog(TS_OFF, VLEVEL_L, "%02x,\n\r", physec_key[i]);
                }
              }

              goto go_to_rx;
            } else {
              if (num_csi >= NUM_MAX_CSI) {
                reset_handler(KG_RST_REQ);
              }
            }
          } else {
            // if the number of the measurements is not sufficient we do
            // nothing!
            tm_plog(TS_ON, VLEVEL_H, ">> Need to do more probing! \n\r");
          }
        }

        send_probe(probe_cnt);
        tm_plog(TS_ON, VLEVEL_M, "> Probe Sent ! (cnt=%u)\n\r", probe_cnt);

        if (!physec_conf.keygen.is_master) {
          probe_cnt++;
        }

        break;
      }
      case PHYSEC_PACKET_TYPE_KEYGEN: {
        if (physec_state >= PHYSEC_STATE_RECONCILIATION) {
          // ignore keygen packet if we are in reconciliation state
          goto go_to_rx;
        }

        physec_keygen_packet_t *kg_pkt =
            (physec_keygen_packet_t *)&(rx_packet->data);
        tm_plog(TS_ON, VLEVEL_M, "> Keygen packet received\n\r");

        switch (handle_keygen(kg_pkt, physec_conf.keygen.is_master)) {
        case KG_QUANT_ERROR: {
          tm_plog(TS_ON, VLEVEL_M, "Quantization error\n\r");
          // An error happens during slave quantization
          if (physec_conf.keygen.is_master) {
            // go back to probing
            physec_state = PHYSEC_STATE_PROBING;
            num_quant_try++;
            num_csi_more = NUM_CSI_MORE_DEFAULT * num_quant_try;
            send_probe(probe_cnt);
          } else {
            // notify master to go back to probing
            physec_packet_t *packet =
                build_keygen_error_packet(BufferTx, MAX_APP_BUFFER_SIZE);
            physec_state = PHYSEC_STATE_PROBING;
            Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
          }
          // if (!QUANT_IS_BLOCKWISE(physec_conf.keygen.quant_type)) {
          //   physec_key_num_bits = -1;
          //   num_indexes = 0;
          // }
          goto go_to_rx;
        }
        case KG_DONE: {
          // send keygen done packet
          if (physec_conf.keygen.is_master) {
            physec_packet_t *packet = build_keygen_success_packet_lossy(
                BufferTx, MAX_APP_BUFFER_SIZE);

            Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
            physec_state = PHYSEC_STATE_PRE_RECONCILIATION;
            num_quant_try = 0;
          }
          break;
        }
        case KG_PP_SEND_LOST: {
          tm_plog(TS_ON, VLEVEL_M, "Send quant indexes (lost ones)\n\r");
          physec_keygen_packet_t *kg_pkt =
              (physec_keygen_packet_t *)&(rx_packet->data);
          physec_keygen_retransmission_req_t *retransmission_req =
              (physec_keygen_retransmission_req_t *)&(kg_pkt->data);
          post_process_send_indexes(retransmission_req->lost_chunks_bitmap);
          goto go_to_rx;
        }
        case KG_PP_SEND_ALL: {
          if (!physec_conf.keygen.is_master) {
            tm_plog(TS_ON, VLEVEL_M, "num key bits = %d, num indexes = %d\n\r",
                    physec_key_num_bits, num_indexes);
            do_post_processing();
            tm_plog(TS_ON, VLEVEL_M, "num key bits = %d, num indexes = %d\n\r",
                    physec_key_num_bits, num_indexes);
          }
          tm_plog(TS_ON, VLEVEL_M, "Send quant indexes (all)\n\r");
          physec_state = PHYSEC_STATE_POST_KEYGEN_SEND;
          post_process_send_indexes(ALL_ONES_MASK);
          time_last_pp_send_all = HAL_GetTick();
          goto go_to_rx;
        }
        case KG_PP_WAIT_MORE:
          tm_plog(TS_ON, VLEVEL_M, "Wait for more quant indexes\n\r");
          physec_state = PHYSEC_STATE_POST_KEYGEN_WAIT;
          goto go_to_rx;
          break;
          // post processing have been handled, wait for more
          // data packet
        case KG_SLAVE_DONE:
          // bob finished quantizing, we can start reconciliation
          physec_state = PHYSEC_STATE_RECONCILIATION;
          // {
          //   uint8_t tmp_key[AES_KEY_SIZE_IN_BYTES] = {
          //       58,  64,  18, 21, 225, 219, 114, 212,
          //       168, 239, 56, 73, 168, 217, 63,  67};
          //   memcpy(physec_key, tmp_key, sizeof(tmp_key));
          // }
          fe_stl_create_and_send_locks();
        default:
          goto go_to_rx;
        }

        if (physec_conf.keygen.is_master) {
          do_post_processing();
          if (QUANT_IS_LOSSY(physec_conf.keygen.quant_type)) {
            if (physec_key_num_bits < REQUIRED_NUM_BITS_PER_KEY) {
              tm_plog(TS_ON, VLEVEL_L,
                      "Not enough bits after Post Processing (%d bits). "
                      "Resetting...\n\r",
                      physec_key_num_bits);
              reset_handler(KG_RST_REQ);
              goto go_to_rx;
            }
          }
        }
        tm_send_key_info(KEY_TYPE_POST_PROCESSING, physec_key,
                         physec_key_num_bits);

        if (!physec_conf.keygen.is_master) {
          // here we are going to enter in reconciliation.
          // compute reconciliation vector according to reconciliation type
          physec_packet_t *packet_kg_done =
              build_keygen_slave_done(BufferTx, MAX_APP_BUFFER_SIZE);
          physec_state = PHYSEC_STATE_PRE_RECONCILIATION;
          Radio.Send(packet_kg_done, physec_packet_get_size(packet_kg_done));
          goto go_to_rx;
          // physec_packet_t *packet = NULL;
          // switch (physec_conf.keygen.recon_type) {
          // case RECON_FE:
          // case RECON_PCS:
          // case RECON_ECC_SS:
          // default:
          //   packet =
          //       build_recon_packet_default(physec_key, AES_KEY_SIZE_IN_BYTES,
          //                                  BufferTx, MAX_APP_BUFFER_SIZE);
          //   break;
          // }
          //
          // if (!packet) {
          //   tm_plog(TS_ON, VLEVEL_M, "Error building RECON packet\n\r");
          //   goto go_to_rx;
          // }
          //
          // tm_send_key_info(KEY_TYPE_RECONCILIATION, physec_key,
          //                  REQUIRED_NUM_BITS_PER_KEY);
          // // TODO: privacy amplification
          // // tm_send_key_info(KEY_TYPE_PRIVACY_AMPLIFICATION, physec_key,
          // // REQUIRED_NUM_BITS_PER_KEY);
          //
          // physec_state = PHYSEC_STATE_KEY_READY;
          // tm_plog(TS_ON, VLEVEL_L, "Key Ready !");
          //
          // Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
        }

        break;
      }
      case PHYSEC_PACKET_TYPE_RECONCILIATION: {
        // if (!physec_conf.keygen.is_master) {
        //   // slave send reconciliation packet, and
        //   // master performs reconciliation
        //   goto go_to_rx;
        // }
        if (!physec_conf.keygen.is_master &&
            physec_state == PHYSEC_STATE_PRE_RECONCILIATION) {
          physec_state = PHYSEC_STATE_RECONCILIATION;
        }

        tm_plog(TS_ON, VLEVEL_M, "> Reconciliation packet received:\n\r");
        physec_recon_packet_t *recon_pkt =
            (physec_recon_packet_t *)&(rx_packet->data);

        bool success = false;
        switch (physec_conf.keygen.recon_type) {
        case RECON_FE_STL:
          // bob is going to try reconciliating
          // {
          //   uint8_t tmp_key[AES_KEY_SIZE_IN_BYTES] = {
          //       122, 96,  82, 21, 161, 219, 242, 212,
          //       160, 239, 56, 73, 168, 209, 63,  65};
          //   memcpy(physec_key, tmp_key, sizeof(tmp_key));
          // }
          success = fe_stl_reproduce_received_locks(recon_pkt);
          physec_packet_t *pkt =
              build_recon_result_packet(BufferTx, MAX_APP_BUFFER_SIZE, success);
          // TODO: handle retransmission
          tm_plog(TS_ON, VLEVEL_M, "sending recon result\r\n");
          Radio.Send((uint8_t *)pkt, physec_packet_get_size(pkt));
          break;
        case RECON_PCS:
        case RECON_ECC_SS:
        default:
          // default case without reconciliation for now
          success = true;
          memcpy(physec_key, recon_pkt->data.key, AES_KEY_SIZE_IN_BYTES);
          break;
        }
        // TODO: Add privacy amplification using ST primitives
        // tm_send_key_info(KEY_TYPE_PRIVACY_AMPLIFICATION, physec_key,
        // REQUIRED_NUM_BITS_PER_KEY);

        if (success) {
          physec_state = PHYSEC_STATE_KEY_READY;
          memcpy(physec_key, recon_key, AES_KEY_SIZE_IN_BYTES);
          tm_plog(TS_ON, VLEVEL_L, "Keys successfully reconciliated !\n\r");
          tm_send_key_info(KEY_TYPE_RECONCILIATION, physec_key,
                           REQUIRED_NUM_BITS_PER_KEY);

        } else {
          tm_plog(TS_ON, VLEVEL_L, " Recon Try Failed !\n\r");
        }

        break;
      }
      case PHYSEC_PACKET_TYPE_RECONCILIATION_RESULT: {
        physec_recon_result_packet_t *result =
            (physec_recon_result_packet_t *)&(rx_packet->data);
        if (result->success) {
          physec_state = PHYSEC_STATE_KEY_READY;
          memcpy(physec_key, recon_key, AES_KEY_SIZE_IN_BYTES);
          tm_send_key_info(KEY_TYPE_RECONCILIATION, physec_key,
                           REQUIRED_NUM_BITS_PER_KEY);

        } else {
          if (recon_num_try < FE_STL_MAX_LOCKS) {
            fe_stl_create_and_send_locks();
          } else {
            reset_handler(KG_RST_REQ);
          }
        }
        goto go_to_rx;
        break;
      }
      case PHYSEC_PACKET_TYPE_ENCRYPTED: {
        tm_plog(TS_ON, VLEVEL_M, "> Encrypted packet received:\n\r");
        if (physec_state == PHYSEC_STATE_KEY_READY) {
          physec_encrypted_packet_t *enc_pkt =
              (physec_encrypted_packet_t *)&(rx_packet->data);

          uint8_t payload[sizeof(SECRET_MSG) + 1];
          if (enc_pkt->size != sizeof(SECRET_MSG)) {
            tm_plog(TS_ON, VLEVEL_M, "Invalid packet size !\n\r");
            goto go_to_rx;
          }
          memcpy(payload, enc_pkt->payload, enc_pkt->size);
          vigenere_encrypt_decrypt(payload, enc_pkt->size, physec_key,
                                   AES_KEY_SIZE_IN_BYTES);
          tm_plog(TS_ON, VLEVEL_L, "Decrypted message: %s\n\r", payload);
          if (memcmp(payload, SECRET_MSG, sizeof(SECRET_MSG)) == 0) {
            tm_plog(TS_ON, VLEVEL_L,
                    "Secret message successfully decrypted !\n\r");
            // Radio.Send(BufferRx, RxBufferSize);

          } else {
            tm_plog(TS_ON, VLEVEL_L, "Decryption failed !\n\r");
          }
          if (physec_conf.keygen.is_master) {
            reset_handler(USER_BTN);
          } else {
            uint8_t msg[sizeof(SECRET_MSG) + 1] = {0};
            memcpy(msg, SECRET_MSG, sizeof(SECRET_MSG));
            vigenere_encrypt_decrypt(msg, sizeof(SECRET_MSG), physec_key,
                                     AES_KEY_SIZE_IN_BYTES);
            physec_packet_t *packet = build_encrypted_packet(
                msg, sizeof(SECRET_MSG), BufferTx, MAX_APP_BUFFER_SIZE);
            Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
          }
        }
        break;
      }
      case PHYSEC_PACKET_TYPE_RESET: {
        tm_plog(TS_ON, VLEVEL_M, "< Reset Packet Received:\n\r");
        physec_reset_packet_t *rst_pck =
            (physec_reset_packet_t *)&rx_packet->data;

        if (!rst_pck->ack)
          reset_handler(RECV_RST);
        else
          random_delay = (Radio.Random()) >> 22;
        break;
      }
      default:
        break;
      }
    } else {
      char msg[MAX_APP_BUFFER_SIZE * 2 + 1] = {0};
      hexlify(BufferRx, RxBufferSize, msg, sizeof(msg));
      tm_plog(TS_ON, VLEVEL_M, "< Invalid PHYsec packet received.\n\r");
      tm_plog(TS_ON, VLEVEL_H, "RAW = %s\n\r", msg);
      tm_plog(TS_ON, VLEVEL_H, "Discarding...\n\r");
    }
  go_to_rx:
    Radio.Rx(RX_TIMEOUT_VALUE + random_delay);
    break;
  case TX:
    Radio.Rx(RX_TIMEOUT_VALUE + random_delay);
    break;
  case RX_TIMEOUT:
  case RX_ERROR:
    // Handle retransmission in case of Error/Timeout
    tm_plog(TS_ON, VLEVEL_M, "[Timeout/Error]\r\n");

    switch (physec_state) {
    case PHYSEC_STATE_PRE_RECONCILIATION: {
      if (!physec_conf.keygen.is_master) {
        physec_packet_t *retrans =
            build_keygen_slave_done(BufferTx, MAX_APP_BUFFER_SIZE);
        Radio.Send(retrans, physec_packet_get_size(retrans));
      }
      break;
    }
    case PHYSEC_STATE_POST_KEYGEN_SEND: {
      // if (physec_conf.keygen.is_master) {
      if (HAL_GetTick() - time_last_pp_send_all > POST_PROCESS_SEND_DELAY_MS) {
        // Send all indexes if no response (assuming slave did
        // not receive any chunks)
        tm_plog(TS_ON, VLEVEL_M, "> Post-Processing (lossy quantization)\n\r");
        // Add lossy points to KEYGEN vector
        num_indexes = (num_indexes > NUM_MAX_QUANT_INDEX) ? NUM_MAX_QUANT_INDEX
                                                          : num_indexes;
        post_process_send_indexes(ALL_ONES_MASK); // send all indexes chunks
        time_last_pp_send_all = HAL_GetTick();
      }
      break;
    }
    case PHYSEC_STATE_POST_KEYGEN_WAIT: {
      // ask for retransmission of lost chunks
      tm_plog(TS_ON, VLEVEL_M, "Requesting 0x%x indexes\n\r",
              (uint16_t)~post_process_rx_chunks);
      physec_packet_t *packet = build_keygen_retransmission_req_packet(
          ~post_process_rx_chunks, BufferTx, MAX_APP_BUFFER_SIZE);

      Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
      HAL_Delay(Radio.GetWakeupTime() + RX_TIME_MARGIN);
      return;
    }
    case PHYSEC_STATE_PROBING: {
      if (physec_conf.keygen.is_master == true) {
        /* Send the next PING frame */
        /* Add delay between RX and TX*/
        /* add random_delay to force sync between boards after some trials*/
        // tm_plog(TS_ON, VLEVEL_L, "Master Tx start\n\r");
        /* master sends PING*/

        // Could not receive probe response, try send back probe_req
        send_probe(probe_cnt);
        tm_plog(TS_ON, VLEVEL_M, "< Probe Sent ! (cnt=%u)\n\r", probe_cnt);
        return;

        // memcpy(BufferTx, PING, sizeof(PING) - 1);
        // Radio.Send(BufferTx, PAYLOAD_LEN);
      } else if (quant_status == QUANT_STATUS_FAILURE) {
        // if quantization was requested but failed,
        // then send KeyGen Quant error packet
        physec_packet_t *packet =
            build_keygen_error_packet(BufferTx, MAX_APP_BUFFER_SIZE);
        Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
      }
      break;
    }
    case PHYSEC_STATE_KEYGEN: {
      if (physec_conf.keygen.is_master) {
        // Send KeyGen start packet for lossless quant
        // (assuming it was not received by slave)
        physec_packet_t *packet =
            build_keygen_success_packet_lossless(BufferTx, MAX_APP_BUFFER_SIZE);
        Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
      }

      break;
    }
    case PHYSEC_STATE_RECONCILIATION: {
      // retransmit current tx buffer !
      // TODO: check if we are always in the case were BufferTx already contains
      // our data

      if (physec_conf.keygen.is_master) {
        tm_plog(TS_ON, VLEVEL_M, "recon rez recon buf\r\n");
        Radio.Send(BufferTx, physec_packet_get_size(BufferTx));
      } else {
        tm_plog(TS_ON, VLEVEL_M, "resending recon rez\r\n");
        Radio.Send(BufferTx, physec_packet_get_size(BufferTx));
      }
      // if (physec_conf.keygen.is_master) {
      //   // indicating Slave need to continue to reconciliation phase
      //   // (assuming it was not received by slave)
      //   tm_plog(TS_ON, VLEVEL_L, "Sending Post-Keygen end packet\n\r");
      //   physec_packet_t *packet =
      //       build_keygen_success_packet_lossy(BufferTx, MAX_APP_BUFFER_SIZE);
      //
      //   // HAL_Delay(Radio.GetWakeupTime() + RX_TIME_MARGIN);
      //   Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
      // } else {
      // }
      break;
    }
    case PHYSEC_STATE_KEY_READY: {
      if (!physec_conf.keygen.is_master)
        break;
      tm_plog(TS_ON, VLEVEL_L, "> Sending encrypted packet\n\r");

      uint8_t msg[sizeof(SECRET_MSG) + 1] = {0};
      memcpy(msg, SECRET_MSG, sizeof(SECRET_MSG));
      vigenere_encrypt_decrypt(msg, sizeof(SECRET_MSG), physec_key,
                               AES_KEY_SIZE_IN_BYTES);
      physec_packet_t *packet = build_encrypted_packet(
          msg, sizeof(SECRET_MSG), BufferTx, MAX_APP_BUFFER_SIZE);

      // HAL_Delay(Radio.GetWakeupTime() + RX_TIME_MARGIN);
      Radio.Send((uint8_t *)packet, physec_packet_get_size(packet));
      break;
    }
    default:
      break;
    }
    Radio.Rx(RX_TIMEOUT_VALUE + random_delay);

    break;
  case TX_TIMEOUT:
    Radio.Rx(RX_TIMEOUT_VALUE + random_delay);
    break;
  default:
    break;
  }
}

static void OnledEvent(void *context) {
  UNUSED(context);
  LED_Toggle(LED_RED1);
  LED_Toggle(LED_RED2);
  UTIL_TIMER_Start(&timerLed);
}

void handle_user_button(void) {
  tm_plog(TS_OFF, VLEVEL_L, "> Restarting key generation\n\r");
  reset_handler(USER_BTN);
}

void reset_physec_states(void) {
  tm_plog(TS_ON, VLEVEL_M, "Performing Reset !\r\n");
  physec_key_num_bits = 0;

  RxBufferSize = 0;
  RssiValue = 0;
  SnrValue = 0;
  time_last_pp_send_all = 0;
  num_reg_rssis = 0;
  last_added_num_reg_rssis = 0;

  num_quantized_csi = 0;
  num_csi = 0;
  num_indexes = 0;
  // num_excursions = 0;
  probe_cnt = 0;
  num_remote_indexes = 0;
  quant_status = QUANT_STATUS_WAITING;

  num_csi_more = 0;
  num_quant_try = 0;

  post_process_expired = false;
  post_process_num_chunks_to_rx = 0;
  post_process_rx_chunks = 0;

  physec_state = PHYSEC_STATE_PROBING;
  recon_num_try = 0;
}

/* USER CODE END PrFD */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF
 * FILE****/
