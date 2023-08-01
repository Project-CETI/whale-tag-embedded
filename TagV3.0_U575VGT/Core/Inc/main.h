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
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void MX_TIM2_Fake_Init(uint8_t newPeriod);
void MX_SDMMC1_SD_Fake_Init(uint8_t newClockDiv);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BMS_ALRT_Pin GPIO_PIN_2
#define BMS_ALRT_GPIO_Port GPIOC
#define VHF_PTT_Pin GPIO_PIN_2
#define VHF_PTT_GPIO_Port GPIOA
#define APRS_DAC_Pin GPIO_PIN_4
#define APRS_DAC_GPIO_Port GPIOA
#define APRS_PD_Pin GPIO_PIN_5
#define APRS_PD_GPIO_Port GPIOA
#define APRS_H_L_Pin GPIO_PIN_6
#define APRS_H_L_GPIO_Port GPIOA
#define GPS_EXTINT_Pin GPIO_PIN_0
#define GPS_EXTINT_GPIO_Port GPIOB
#define ADC_CS_Pin GPIO_PIN_2
#define ADC_CS_GPIO_Port GPIOB
#define IMU_WAKE_Pin GPIO_PIN_11
#define IMU_WAKE_GPIO_Port GPIOE
#define IMU_INT_Pin GPIO_PIN_12
#define IMU_INT_GPIO_Port GPIOE
#define IMU_INT_EXTI_IRQn EXTI12_IRQn
#define IMU_CS_Pin GPIO_PIN_12
#define IMU_CS_GPIO_Port GPIOB
#define KELLER_EOC_Pin GPIO_PIN_13
#define KELLER_EOC_GPIO_Port GPIOB
#define ECG_LOD__Pin GPIO_PIN_10
#define ECG_LOD__GPIO_Port GPIOD
#define ECG_LOD_D11_Pin GPIO_PIN_11
#define ECG_LOD_D11_GPIO_Port GPIOD
#define ECG_NDRDY_Pin GPIO_PIN_14
#define ECG_NDRDY_GPIO_Port GPIOD
#define ECG_NDRDY_EXTI_IRQn EXTI14_IRQn
#define ECG_NSDN_Pin GPIO_PIN_15
#define ECG_NSDN_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
