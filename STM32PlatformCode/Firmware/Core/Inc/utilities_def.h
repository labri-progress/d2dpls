/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    utilities_def.h
 * @author  MCD Application Team
 * @brief   Definitions for modules requiring utilities
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
#ifndef __UTILITIES_DEF_H__
#define __UTILITIES_DEF_H__

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/******************************************************************************
 * LOW POWER MANAGER
 ******************************************************************************/
/**
 * Supported requester to the MCU Low Power Manager - can be increased up  to 32
 * It lists a bit mapping of all user of the Low Power Manager
 */
typedef enum {
  /* USER CODE BEGIN CFG_LPM_Id_t_0 */

  /* USER CODE END CFG_LPM_Id_t_0 */
  CFG_LPM_APPLI_Id,
  CFG_LPM_RTC_Id,
  CFG_LPM_UART_TX_Id,
  CFG_LPM_TCXO_WA_Id,
  /* USER CODE BEGIN CFG_LPM_Id_t */

  /* USER CODE END CFG_LPM_Id_t */
} CFG_LPM_Id_t;

/*---------------------------------------------------------------------------*/
/*                             sequencer definitions                         */
/*---------------------------------------------------------------------------*/

/**
 * This is the list of priority required by the application
 * Each Id shall be in the range 0..31
 */
typedef enum {
  CFG_SEQ_Prio_0,
  /* USER CODE BEGIN CFG_SEQ_Prio_Id_t */

  /* USER CODE END CFG_SEQ_Prio_Id_t */
  CFG_SEQ_Prio_NBR,
} CFG_SEQ_Prio_Id_t;

/**
 * This is the list of task id required by the application
 * Each Id shall be in the range 0..31
 */
typedef enum {
  CFG_SEQ_Task_SubGHz_Phy_App_Process,
  /* USER CODE BEGIN CFG_SEQ_Task_Id_t */

  /* USER CODE END CFG_SEQ_Task_Id_t */
  CFG_SEQ_Task_NBR
} CFG_SEQ_Task_Id_t;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */
/**
 * This macro initializes the GPIOA0 pin
 */
#define HAL_GPIO_SIGNAL_INIT()                                                 \
  do {                                                                         \
    /* enabling the GPIOA lane clock */                                        \
    __HAL_RCC_GPIOA_CLK_ENABLE();                                              \
    /* initialization parameters (pin 0, pull/push, pulldown, high freq) */    \
    GPIO_InitTypeDef gpio_a0_init = {0};                                       \
    gpio_a0_init.Pin = GPIO_PIN_0;                                             \
    gpio_a0_init.Mode = GPIO_MODE_OUTPUT_PP;                                   \
    gpio_a0_init.Pull = GPIO_PULLDOWN;                                           \
    gpio_a0_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;                            \
    HAL_GPIO_Init(GPIOA, &gpio_a0_init);                                       \
  } while (0)

/**
 * This macro sets the GPIOA0 pin to high for n_cycles cycles then to low
 *
 * As up for now it can be call between 3 and 4 times per keygen.
 *
 * On KG start, first probe sent / received
 * On reconciliation start
 * On key ready
 * On RST
 *
 * TODO:
 * - Discuss about what happens if we replay, shall we still be able to measure ? (=> if so we set the probing to one)
 * - do we need Key Ready / RST signals or just one single kg end when key is ready?
 */
#define HAL_GPIO_SIGNAL_TRIGGER(n_cycles)                                      \
  do {                                                                         \
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);                        \
    for (size_t cycle_idx = 0; cycle_idx < n_cycles; cycle_idx++) {            \
      __NOP();                                                                 \
    }                                                                          \
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);                      \
  } while (0)

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __UTILITIES_DEF_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
