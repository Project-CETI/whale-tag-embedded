/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_cdc_acm.h
  * @author  MCD Application Team
  * @brief   USBX Device CDC ACM interface header file
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
#ifndef __UX_DEVICE_CDC_ACM_H__
#define __UX_DEVICE_CDC_ACM_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ux_api.h"
#include "ux_device_class_cdc_acm.h"

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
VOID USBD_CDC_ACM_Activate(VOID *cdc_acm_instance);
VOID USBD_CDC_ACM_Deactivate(VOID *cdc_acm_instance);
VOID USBD_CDC_ACM_ParameterChange(VOID *cdc_acm_instance);

/* USER CODE BEGIN EFP */
VOID usbx_cdc_acm_read_thread_entry(ULONG thread_input);
VOID usbx_cdc_acm_write_thread_entry(ULONG thread_input);
UINT ux_cdc_write_callback(UX_SLAVE_CLASS_CDC_ACM *cdc_acm, UINT status, ULONG length);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */

// configuration for usb-uart
#define USB_UART_BAUDRATE			9600
#define MIN_BAUDRATE				9600
#define VCP_WORDLENGTH8				8
#define APP_RX_DATA_SIZE			64
#define APP_TX_DATA_SIZE			64

// usb flags
#define USB_TRANSMIT_FLAG			0x1

// usb command start char
#define USB_START_CHAR				0x24

// usb command ids
#define USB_SLEEP_CMD				0x1
#define USB_WAKE_CMD				0x2
#define USB_TEST_SENSORS_CMD		0x3
#define USB_READ_SENSOR_CMD			0x4
#define USB_WRITE_SENSOR_CMD		0x5
#define USB_CMD_SENSOR_CMD			0x6
#define USB_SIM_CMD					0x7

// sensor ids
#define IMU_SENSOR_ID				0x1
#define BMS_SENSOR_ID				0x2
#define AUDIO_SENSOR_ID				0x3
#define ECG_SENSOR_ID				0x4
#define DEPTH_SENSOR_ID				0x5
#define LIGHT_SENSOR_ID				0x6
#define RTC_SENSOR_ID				0x7

// subcommands for simulation command
#define SIM_EXIT_CMD				0x1
#define SIM_ENTER_CMD				0x2

// timeout constants
#define USB_UNIT_TEST_TIMEOUT		tx_s_to_ticks(30)

/* USER CODE END PD */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif  /* __UX_DEVICE_CDC_ACM_H__ */
