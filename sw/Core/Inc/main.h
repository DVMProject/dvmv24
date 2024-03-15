/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define PROG_NAME "DVM-V24"
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_HB_Pin GPIO_PIN_13
#define LED_HB_GPIO_Port GPIOC
#define USB_RENUM_Pin GPIO_PIN_6
#define USB_RENUM_GPIO_Port GPIOA
#define LED_LINK_Pin GPIO_PIN_12
#define LED_LINK_GPIO_Port GPIOB
#define LED_ACT_Pin GPIO_PIN_13
#define LED_ACT_GPIO_Port GPIOB
#define LED_USB_Pin GPIO_PIN_14
#define LED_USB_GPIO_Port GPIOB
#define DCE_RXCLK_Pin GPIO_PIN_3
#define DCE_RXCLK_GPIO_Port GPIOB
#define DCE_RXD_Pin GPIO_PIN_4
#define DCE_RXD_GPIO_Port GPIOB
#define DCE_RTS_Pin GPIO_PIN_5
#define DCE_RTS_GPIO_Port GPIOB
#define DCE_TXCLK_Pin GPIO_PIN_6
#define DCE_TXCLK_GPIO_Port GPIOB
#define DCE_TXD_Pin GPIO_PIN_7
#define DCE_TXD_GPIO_Port GPIOB
#define DCE_CTS_Pin GPIO_PIN_8
#define DCE_CTS_GPIO_Port GPIOB
#define DCE_DTR_Pin GPIO_PIN_9
#define DCE_DTR_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
