/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_cdc_acm.c
  * @author  MCD Application Team
  * @brief   USBX Device applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "ux_device_cdc_acm.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Lib Inc/state_machine.h"
#include "Sensor Inc/BMS.h"
#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/ECG.h"
#include "Sensor Inc/KellerDepth.h"
#include "Sensor Inc/LightSensor.h"
#include "Sensor Inc/audio.h"
#include "Sensor Inc/RTC.h"
#include "app_usbx_device.h"
#include "main.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
UX_SLAVE_CLASS_CDC_ACM	*cdc_acm;

uint8_t usbReceiveBuf[APP_RX_DATA_SIZE];
uint8_t usbTransmitBuf[APP_TX_DATA_SIZE];
uint16_t usbTransmitLen = 0;

// usb flags
TX_EVENT_FLAGS_GROUP usb_cdc_event_flags_group;

extern UART_HandleTypeDef huart2;

// usb read and write threads
extern TX_THREAD ux_cdc_read_thread;
extern TX_THREAD ux_cdc_write_thread;

// thread flags
extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern TX_EVENT_FLAGS_GROUP bms_event_flags_group;
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_EVENT_FLAGS_GROUP depth_event_flags_group;
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_EVENT_FLAGS_GROUP audio_event_flags_group;
extern TX_EVENT_FLAGS_GROUP light_event_flags_group;
extern TX_EVENT_FLAGS_GROUP rtc_event_flags_group;

// unit test status variables
extern HAL_StatusTypeDef imu_unit_test_ret;
extern HAL_StatusTypeDef bms_unit_test_ret;
extern HAL_StatusTypeDef audio_unit_test_ret;
extern HAL_StatusTypeDef ecg_unit_test_ret;
extern HAL_StatusTypeDef depth_unit_test_ret;
extern HAL_StatusTypeDef light_unit_test_ret;
extern HAL_StatusTypeDef rtc_unit_test_ret;

// configuration
extern bool sleep_state[10];

// usb configuration
UX_SLAVE_CLASS_CDC_ACM_LINE_CODING_PARAMETER CDC_VCP_LineCoding =
{
		9600, /* baud rate */
		0x00,   /* stop bits-1 */
		0x00,   /* parity - none */
		0x08    /* nb. of bits 8 */
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  USBD_CDC_ACM_Activate
  *         This function is called when insertion of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Activate(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Activate */
  //UX_PARAMETER_NOT_USED(cdc_acm_instance);

  // check if tag in USB CDC mode
  if (sleep_state[1]) {
    cdc_acm = (UX_SLAVE_CLASS_CDC_ACM*) cdc_acm_instance;

    CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_baudrate = USB_UART_BAUDRATE;
    CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_data_bit = VCP_WORDLENGTH8;
    CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_parity = UART_PARITY_NONE;
    CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_stop_bit = UART_STOPBITS_1;

    if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING, &CDC_VCP_LineCoding) != UX_SUCCESS) {
      Error_Handler();
    }

    // create the events flag
    tx_event_flags_create(&usb_cdc_event_flags_group, "USB CDC Event Flags");

    // start usb read and write thread
    //tx_thread_resume(&ux_cdc_read_thread);
    //tx_thread_resume(&ux_cdc_write_thread);
  }

  /* USER CODE END USBD_CDC_ACM_Activate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_Deactivate
  *         This function is called when extraction of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Deactivate(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Deactivate */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);

  cdc_acm = UX_NULL;

  // suspend usb read and write thread
  tx_thread_suspend(&ux_cdc_read_thread);
  tx_thread_suspend(&ux_cdc_write_thread);

  /* USER CODE END USBD_CDC_ACM_Deactivate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_ParameterChange
  *         This function is invoked to manage the CDC ACM class requests.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_ParameterChange(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_ParameterChange */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);

  ULONG request;
  UX_SLAVE_TRANSFER *transfer_request;
  UX_SLAVE_DEVICE *device;

  // get pointer to device
  device = &_ux_system_slave -> ux_system_slave_device;

  // get pointer to transfer request associated with control endpoint
  transfer_request = &device -> ux_slave_device_control_endpoint.ux_slave_endpoint_transfer_request;

  request = *(transfer_request -> ux_slave_transfer_request_setup + UX_SETUP_REQUEST);

  switch (request) {
    case UX_SLAVE_CLASS_CDC_ACM_SET_LINE_CODING :

	  /* Get the Line Coding parameters */
	  if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_GET_LINE_CODING, &CDC_VCP_LineCoding) != UX_SUCCESS) {
	    Error_Handler();
	  }

	  /* Check if baudrate < 9600 set it to 9600 */
	  if (CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_baudrate < MIN_BAUDRATE) {
	    CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_baudrate = MIN_BAUDRATE;
	  }

	  break;

    case UX_SLAVE_CLASS_CDC_ACM_GET_LINE_CODING :

	  /* Set the Line Coding parameters */
	  if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING, &CDC_VCP_LineCoding) != UX_SUCCESS) {
	    Error_Handler();
	  }

	  break;

    case UX_SLAVE_CLASS_CDC_ACM_SET_CONTROL_LINE_STATE :
    default :
	  break;
  }

  /* USER CODE END USBD_CDC_ACM_ParameterChange */

  return;
}

/* USER CODE BEGIN 1 */
VOID usbx_cdc_acm_read_thread_entry(ULONG thread_input) {

  ULONG actual_length = 0;
  UX_SLAVE_DEVICE *device;

  UX_PARAMETER_NOT_USED(thread_input);

  device = &_ux_system_slave->ux_system_slave_device;

  while (1) {
    if ((device->ux_slave_device_state == UX_DEVICE_CONFIGURED) && (cdc_acm != UX_NULL)) {
      #ifndef UX_DEVICE_CLASS_CDC_ACM_TRANSMISSION_DISABLE

      cdc_acm -> ux_slave_class_cdc_acm_transmission_status = UX_FALSE;

      #endif

      ULONG actual_length = 0;

      // read received data in blocking mode
      ux_device_class_cdc_acm_read(cdc_acm, (UCHAR *) &usbReceiveBuf[0], 7, &actual_length);

      /*
       * COMMAND FRAME FORMAT: $<cmd (1 byte)><args (5 bytes)>
       * usbReceiveBuf[0]	:	$ or 0x24
       * usbReceiveBuf[1]	:	cmd
       * usbReceiveBuf[2]	:	arg byte 1
       * usbReceiveBuf[3]	:	arg byte 2
       * usbReceiveBuf[4]	:	arg byte 3
       * usbReceiveBuf[5]	:	arg byte 4
       * usbReceiveBuf[6]	:	arg byte 5
       */

      if (actual_length == 7 && usbReceiveBuf[0] == USB_START_CHAR) {
        switch (usbReceiveBuf[1]) {
          case USB_SLEEP_CMD:
        	  tx_event_flags_set(&state_machine_event_flags_group, STATE_SLEEP_MODE_FLAG, TX_OR);
        	  break;
          case USB_WAKE_CMD:
        	  tx_event_flags_set(&state_machine_event_flags_group, STATE_EXIT_SLEEP_MODE_FLAG, TX_OR);
        	  break;
          case USB_TEST_SENSORS_CMD:
        	  ULONG actual_flags = 0;

        	  // send unit test flag for all sensors
        	  tx_event_flags_set(&imu_event_flags_group, IMU_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_get(&imu_event_flags_group, IMU_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, USB_UNIT_TEST_TIMEOUT);
        	  if (actual_flags & IMU_UNIT_TEST_DONE_FLAG) {
				  memcpy(&usbTransmitBuf[0], &imu_unit_test_ret, 1);
				  usbTransmitLen++;
				  imu_unit_test_ret = HAL_ERROR;
			  }
        	  else {
        		  imu_unit_test_ret = HAL_TIMEOUT;
        		  memcpy(&usbTransmitBuf[0], &imu_unit_test_ret, 1);
        		  usbTransmitLen++;
        	  }

        	  tx_event_flags_set(&bms_event_flags_group, BMS_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_get(&bms_event_flags_group, BMS_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, USB_UNIT_TEST_TIMEOUT);
        	  if (actual_flags & BMS_UNIT_TEST_DONE_FLAG) {
        		  memcpy(&usbTransmitBuf[1], &bms_unit_test_ret, 1);
        		  usbTransmitLen++;
        		  bms_unit_test_ret = HAL_ERROR;
        	  }
        	  else {
        		  bms_unit_test_ret = HAL_TIMEOUT;
        		  memcpy(&usbTransmitBuf[1], &bms_unit_test_ret, 1);
        		  usbTransmitLen++;
        	  }

        	  tx_event_flags_set(&audio_event_flags_group, AUDIO_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_get(&audio_event_flags_group, AUDIO_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, USB_UNIT_TEST_TIMEOUT);
        	  if (actual_flags & AUDIO_UNIT_TEST_DONE_FLAG) {
				  memcpy(&usbTransmitBuf[2], &audio_unit_test_ret, 1);
				  usbTransmitLen++;
				  // NOTE: audio unit test not reset to HAL_ERROR because it is only run once
			  }
        	  else {
        		  audio_unit_test_ret = HAL_TIMEOUT;
        		  memcpy(&usbTransmitBuf[2], &audio_unit_test_ret, 1);
				  usbTransmitLen++;
        	  }

        	  tx_event_flags_set(&ecg_event_flags_group, ECG_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_get(&ecg_event_flags_group, ECG_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, USB_UNIT_TEST_TIMEOUT);
        	  if (actual_flags & ECG_UNIT_TEST_DONE_FLAG) {
				  memcpy(&usbTransmitBuf[3], &ecg_unit_test_ret, 1);
				  usbTransmitLen++;
				  ecg_unit_test_ret = HAL_ERROR;
			  }
        	  else {
        		  ecg_unit_test_ret = HAL_TIMEOUT;
        		  memcpy(&usbTransmitBuf[3], &ecg_unit_test_ret, 1);
				  usbTransmitLen++;
        	  }

        	  tx_event_flags_set(&depth_event_flags_group, DEPTH_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_get(&depth_event_flags_group, DEPTH_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, USB_UNIT_TEST_TIMEOUT);
        	  if (actual_flags & DEPTH_UNIT_TEST_DONE_FLAG) {
				  memcpy(&usbTransmitBuf[4], &depth_unit_test_ret, 1);
				  usbTransmitLen++;
				  depth_unit_test_ret = HAL_ERROR;
			  }
        	  else {
        		  depth_unit_test_ret = HAL_TIMEOUT;
        		  memcpy(&usbTransmitBuf[4], &depth_unit_test_ret, 1);
        		  usbTransmitLen++;
        	  }

        	  tx_event_flags_set(&light_event_flags_group, LIGHT_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_get(&light_event_flags_group, LIGHT_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, USB_UNIT_TEST_TIMEOUT);
        	  if (actual_flags & LIGHT_UNIT_TEST_DONE_FLAG) {
				  memcpy(&usbTransmitBuf[5], &light_unit_test_ret, 1);
				  usbTransmitLen++;
				  light_unit_test_ret = HAL_ERROR;
			  }
        	  else {
        		  light_unit_test_ret = HAL_TIMEOUT;
				  memcpy(&usbTransmitBuf[5], &light_unit_test_ret, 1);
				  usbTransmitLen++;
        	  }

        	  tx_event_flags_set(&rtc_event_flags_group, RTC_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_get(&rtc_event_flags_group, RTC_UNIT_TEST_DONE_FLAG, TX_OR_CLEAR, &actual_flags, USB_UNIT_TEST_TIMEOUT);
        	  if (actual_flags & RTC_UNIT_TEST_DONE_FLAG) {
				  memcpy(&usbTransmitBuf[6], &rtc_unit_test_ret, 1);
				  usbTransmitLen++;
				  rtc_unit_test_ret = HAL_ERROR;
			  }
        	  else {
        		  rtc_unit_test_ret = HAL_TIMEOUT;
				  memcpy(&usbTransmitBuf[6], &rtc_unit_test_ret, 1);
				  usbTransmitLen++;
        	  }

        	  // tell usb transmit thread to transmit data
        	  tx_event_flags_set(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR);

        	  break;
          case USB_READ_SENSOR_CMD:
        	  // send read flag to respective sensor
        	  if (usbReceiveBuf[2] == BMS_SENSOR_ID) {
        		  tx_event_flags_set(&bms_event_flags_group, BMS_READ_FLAG, TX_OR);
        	  }
        	  else if (usbReceiveBuf[2] == AUDIO_SENSOR_ID) {
				  tx_event_flags_set(&audio_event_flags_group, AUDIO_READ_FLAG, TX_OR);
			  }
        	  else if (usbReceiveBuf[2] == ECG_SENSOR_ID) {
        		  tx_event_flags_set(&ecg_event_flags_group, ECG_READ_FLAG, TX_OR);
        	  }
        	  else if (usbReceiveBuf[2] == LIGHT_SENSOR_ID) {
				  tx_event_flags_set(&light_event_flags_group, LIGHT_READ_FLAG, TX_OR);
			  }
        	  break;
          case USB_WRITE_SENSOR_CMD:
        	  // send write flag to respective sensor
			  if (usbReceiveBuf[2] == BMS_SENSOR_ID) {
				  tx_event_flags_set(&bms_event_flags_group, BMS_WRITE_FLAG, TX_OR);
			  }
			  else if (usbReceiveBuf[2] == ECG_SENSOR_ID) {
				  tx_event_flags_set(&ecg_event_flags_group, ECG_WRITE_FLAG, TX_OR);
			  }
			  else if (usbReceiveBuf[2] == LIGHT_SENSOR_ID) {
				  tx_event_flags_set(&light_event_flags_group, LIGHT_WRITE_FLAG, TX_OR);
			  }
			  break;
          case USB_CMD_SENSOR_CMD:
        	  // send command flag to respective sensor
        	  if (usbReceiveBuf[2] == IMU_SENSOR_ID) {
				  tx_event_flags_set(&imu_event_flags_group, IMU_CMD_FLAG, TX_OR);
			  }
			  else if (usbReceiveBuf[2] == BMS_SENSOR_ID) {
				  tx_event_flags_set(&bms_event_flags_group, BMS_CMD_FLAG, TX_OR);
			  }
			  else if (usbReceiveBuf[2] == AUDIO_SENSOR_ID) {
				  tx_event_flags_set(&audio_event_flags_group, AUDIO_CMD_FLAG, TX_OR);
			  }
			  else if (usbReceiveBuf[2] == ECG_SENSOR_ID) {
				  tx_event_flags_set(&ecg_event_flags_group, ECG_CMD_FLAG, TX_OR);
			  }
			  else if (usbReceiveBuf[2] == DEPTH_SENSOR_ID) {
				  tx_event_flags_set(&depth_event_flags_group, DEPTH_CMD_FLAG, TX_OR);
			  }
			  else if (usbReceiveBuf[2] == LIGHT_SENSOR_ID) {
				  tx_event_flags_set(&light_event_flags_group, LIGHT_CMD_FLAG, TX_OR);
			  }
			  else if (usbReceiveBuf[2] == RTC_SENSOR_ID) {
				  tx_event_flags_set(&rtc_event_flags_group, RTC_CMD_FLAG, TX_OR);
			  }
			  break;
          case USB_SIM_CMD:
        	  // exit simulation mode
        	  if (usbReceiveBuf[2] == SIM_EXIT_CMD) {
        		  tx_event_flags_set(&state_machine_event_flags_group, STATE_EXIT_SIM_FLAG, TX_OR);
        	  }
        	  // enter simulation mode
        	  else if (usbReceiveBuf[2] == SIM_ENTER_CMD) {
        		  tx_event_flags_set(&state_machine_event_flags_group, STATE_ENTER_SIM_FLAG, TX_OR);
        	  }
          default:
        	  // clear receive buffer if invalid command to avoid errors
        	  usbReceiveBuf[0] = 0;
        	  usbReceiveBuf[1] = 0;
        	  usbReceiveBuf[2] = 0;
        	  usbReceiveBuf[3] = 0;
        	  usbReceiveBuf[4] = 0;
        	  usbReceiveBuf[5] = 0;
        	  usbReceiveBuf[6] = 0;
        	  break;
    	}
      }
      else {
    	  tx_thread_sleep(MS_TO_TICK(10));
      }
    }
    else {
    	tx_thread_sleep(MS_TO_TICK(10));
    }
  }
}

VOID usbx_cdc_acm_write_thread_entry(ULONG thread_input) {

	UX_PARAMETER_NOT_USED(thread_input);

	#ifndef UX_DEVICE_CLASS_CDC_ACM_TRANSMISSION_DISABLE

		/* Set transmission_status to UX_FALSE for the first time */
		cdc_acm -> ux_slave_class_cdc_acm_transmission_status = UX_FALSE;

	#endif

	while (1) {
		ULONG actual_flags = 0;

		tx_event_flags_get(&usb_cdc_event_flags_group, USB_TRANSMIT_FLAG, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
		if (actual_flags & USB_TRANSMIT_FLAG) {
			if (usbTransmitLen > 0) {
				ULONG actual_length = 0;
				if (ux_device_class_cdc_acm_write(cdc_acm, &usbTransmitBuf[0], usbTransmitLen, &actual_length)) {
					usbTransmitLen = 0;
				}
			}
			else {
				tx_thread_sleep(MS_TO_TICK(10));
			}
		}
		else {
			tx_thread_sleep(MS_TO_TICK(10));
		}
	}
}

/* USER CODE END 1 */
