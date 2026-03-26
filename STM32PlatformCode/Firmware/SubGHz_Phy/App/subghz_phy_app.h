/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    subghz_phy_app.h
 * @author  MCD Application Team
 * @brief   Header of application of the SubGHz_Phy Middleware
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SUBGHZ_PHY_APP_H__
#define __SUBGHZ_PHY_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "libphysec/libphysec.h"
#include "physec_config.h"
#include "physec_telemetry.h"
#include <stdint.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* MODEM type: one shall be 1 the other shall be 0 */
#define USE_MODEM_LORA 1
#define USE_MODEM_FSK 0

#define REGION_EU868

#if defined(REGION_AS923)

#define RF_FREQUENCY 923000000 /* Hz */
#elif defined(REGION_AU915)

#define RF_FREQUENCY 915000000 /* Hz */

#elif defined(REGION_CN470)

#define RF_FREQUENCY 470000000 /* Hz */

#elif defined(REGION_CN779)

#define RF_FREQUENCY 779000000 /* Hz */

#elif defined(REGION_EU433)

#define RF_FREQUENCY 433000000 /* Hz */

#elif defined(REGION_EU868)

#define RF_FREQUENCY 868000000 /* Hz */

#elif defined(REGION_KR920)

#define RF_FREQUENCY 920000000 /* Hz */

#elif defined(REGION_IN865)

#define RF_FREQUENCY 865000000 /* Hz */

#elif defined(REGION_US915)

#define RF_FREQUENCY 915000000 /* Hz */

#elif defined(REGION_RU864)

#define RF_FREQUENCY 864000000 /* Hz */

#else
#error "Please define a frequency band in the compiler options."
#endif /* REGION_XXxxx */

#define TX_OUTPUT_POWER 17 /* dBm */

#if ((USE_MODEM_LORA == 1) && (USE_MODEM_FSK == 0))
#define LORA_BANDWIDTH                                                         \
  0 /* [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]                       \
     */
#define LORA_SPREADING_FACTOR 7 /* [SF7..SF12] */
#define LORA_CODINGRATE 1       /* [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8] */
#define LORA_PREAMBLE_LENGTH 8  /* Same for Tx and Rx */
#define LORA_SYMBOL_TIMEOUT 5   /* Symbols */
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#elif ((USE_MODEM_LORA == 0) && (USE_MODEM_FSK == 1))

#define FSK_FDEV 25000        /* Hz */
#define FSK_DATARATE 50000    /* bps */
#define FSK_BANDWIDTH 50000   /* Hz */
#define FSK_PREAMBLE_LENGTH 5 /* Same for Tx and Rx */
#define FSK_FIX_LENGTH_PAYLOAD_ON false

#else
#error "Please define a modem in the compiler subghz_phy_app.h."
#endif /* USE_MODEM_LORA | USE_MODEM_FSK */

/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Configurations */
/*Timeout*/
#define RX_TIMEOUT_VALUE 3000
#define TX_TIMEOUT_VALUE 3000
#define SECRET_MSG "S3Cr3tM3ss4g3"
#define LOG_MSG_MAX_SIZE 512

/*Size of the payload to be sent*/
#define MAX_APP_BUFFER_SIZE 255
#if (PAYLOAD_LEN > MAX_APP_BUFFER_SIZE)
#error PAYLOAD_LEN must be less or equal than MAX_APP_BUFFER_SIZE
#endif /* (PAYLOAD_LEN > MAX_APP_BUFFER_SIZE) */
/* wait for remote to be in Rx, before sending a Tx frame*/
#define RX_TIME_MARGIN 50
/* Afc bandwidth in Hz */
#define FSK_AFC_BANDWIDTH 83333
/* LED blink Period*/
#define LED_PERIOD_MS 200

/* PHYsec defines */
#define POST_PROCESS_SEND_DELAY_MS (10000)

#define QUANT_SB_EXCURSION_MAX_M 8 // fully arbitrary
#define QUANT_SB_EXCURSION_M 3     // MODIFY THIS TO CHANGE
// NUMBER OF MEASURES IN AN
// EXCURSION
#define QUANT_SB_EXCURSION_ALPHA 0.1 // MODIFY THIS TO CHANGE
// NUMBER OF MEASURES IN AN
// EXCURSION
#define QUANT_MB_EXCURSION_M QUANT_SB_EXCURSION_M

#define QUANT_MBE_LOSSY_ALPHA 0.1 // MODIFY THIS TO CHANGE
// THE DATA TO BAND RATIO
// Number of bits per key (should be equal to `sizeof(physec_key) * 8`)
#define QUANT_MOVING_WINDOW_SIZE 10
// extra large capacity to handle lossy quantization
// that produce lot of bits
#define KEY_CAPACITY_IN_BYTES 256
#define AES_KEY_SIZE_IN_BYTES 16
#define REQUIRED_NUM_BITS_PER_KEY (AES_KEY_SIZE_IN_BYTES * 8)

#define NUM_MAX_CSI 1600 // fully arbitrary for now
#define NUM_MAX_LOSSY (NUM_MAX_CSI / 2)
#define NUM_MAX_EXCURSIONS(m) (NUM_MAX_CSI / (m))
#define NUM_MAX_QUANT_INDEX                                                    \
  NUM_MAX_LOSSY // MAX(NUM_MAX_LOSSY, NUM_MAX_EXCURSIONS(m)
// but m=1 is not possible, thus the number
// of lossy points is the max

#define NUM_CSI_MORE_DEFAULT 10

#define NUM_KEPT_AR_RSSI 10
#define UART_CONFIG_TIME_WINDOW_MS 5000

#define MAX_UART_BUF_SIZE 128

#define TM_KEYGEN_CONF_MAX_SIZE                                                \
  (sizeof(physec_telemetry_packet_t) + sizeof(physec_config))
#define TM_KEYGEN_INFO_MAX_SIZE                                                \
  (KEY_CAPACITY_IN_BYTES + sizeof(physec_telemetry_packet_t) +                 \
   sizeof(physec_telemetry_keygen_info_t))

#define ALL_ONES_MASK (~0)

#define PLS_TELEMETRY_ENABLED
#ifdef PLS_TELEMETRY_ENABLED
#define tm_plog(ts, level, fmt, ...)                                           \
  physec_telemetry_log(ts, fmt, ##__VA_ARGS__)
#define tm_send_csis(csis, num) physec_telemetry_send_csis(csis, num)
#define tm_send_keygen_conf() physec_telemetry_send_keygen_conf(&physec_conf)
#define tm_send_key_info(key_type, key, num_bits)                              \
  physec_telemetry_send_keygen_info(key_type, key, num_bits)
#else
#define tm_plog(ts, level, fmt, ...) APP_LOG(ts, level, fmt, ##__VA_ARGS__)
#define tm_send_csis(csis, num_csis)
#define tm_send_key_info()
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/**
 * @brief  Init Subghz Application
 */
void SubghzApp_Init(void);

void logger(const char *fmt, va_list args);
/* USER CODE BEGIN EFP */
void handle_user_button(void);

/**
 * @brief Reset the PhySec states to default
 */
void reset_physec_states(void);
void wait_physec_config(void);

void PHYsec_Platform_Process(void);

enum {
  QUANT_STATUS_WAITING,
  QUANT_STATUS_FAILURE,
  QUANT_STATUS_DONE,
};

enum {
  // waiting for probe packet
  // to respond to
  PHYSEC_STATE_PROBING,
  // wait for reconciliation to happen
  PHYSEC_STATE_KEYGEN,
  // send lossy points to be dropped
  // or excursion indexes to select,
  // handle retransmission for lost
  // packets
  PHYSEC_STATE_POST_KEYGEN_SEND,
  // wait lossy points to be dropped
  // or excursion indexes to select
  PHYSEC_STATE_POST_KEYGEN_WAIT,
  // reconciliation state (not used in this project)
  PHYSEC_STATE_PRE_RECONCILIATION,
  PHYSEC_STATE_RECONCILIATION,
  // Final State, symmetric encryption possible
  PHYSEC_STATE_KEY_READY
};

// Return codes for handle_keygen
enum {
  KG_QUANT_ERROR,
  KG_PP_SEND_LOST,
  KG_PP_SEND_ALL,
  KG_PP_WAIT_MORE,
  KG_DONE,
  KG_SLAVE_DONE,
};

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*__SUBGHZ_PHY_APP_H__*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
