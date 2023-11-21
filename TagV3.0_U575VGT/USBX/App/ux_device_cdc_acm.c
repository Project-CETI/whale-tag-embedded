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

extern TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;
extern TX_EVENT_FLAGS_GROUP bms_event_flags_group;
extern TX_EVENT_FLAGS_GROUP imu_event_flags_group;
extern TX_EVENT_FLAGS_GROUP depth_event_flags_group;
extern TX_EVENT_FLAGS_GROUP ecg_event_flags_group;
extern TX_EVENT_FLAGS_GROUP audio_event_flags_group;
extern TX_EVENT_FLAGS_GROUP light_event_flags_group;
extern TX_EVENT_FLAGS_GROUP rtc_event_flags_group;

UX_SLAVE_CLASS_CDC_ACM_LINE_CODING_PARAMETER CDC_VCP_LineCoding =
{
		9600, /* baud rate */
		0x00,   /* stop bits-1 */
		0x00,   /* parity - none */
		0x08    /* nb. of bits 8 */
};

uint8_t SET_SIMULATION_STATE = 0;

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

  cdc_acm = (UX_SLAVE_CLASS_CDC_ACM*) cdc_acm_instance;

  CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_baudrate = USB_UART_BAUDRATE;
  CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_data_bit = VCP_WORDLENGTH8;
  CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_parity = UART_PARITY_NONE;
  CDC_VCP_LineCoding.ux_slave_class_cdc_acm_parameter_stop_bit = UART_STOPBITS_1;

  if (ux_device_class_cdc_acm_ioctl(cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING, &CDC_VCP_LineCoding) != UX_SUCCESS) {
    Error_Handler();
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

	  /* Check if baudrate < 9600) then set it to 9600 */
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

      // read received data in blocking mode
      ux_device_class_cdc_acm_read(cdc_acm, (UCHAR *) &usbReceiveBuf[0], 6, &actual_length);

      if (actual_length != 0 && usbReceiveBuf[0] == START_CHAR) {
        switch (usbReceiveBuf[1]) {
          case SLEEP_CMD:
        	  tx_event_flags_set(&state_machine_event_flags_group, STATE_SLEEP_MODE_FLAG, TX_OR);
        	  break;
          case WAKE_CMD:
        	  tx_event_flags_set(&state_machine_event_flags_group, STATE_EXIT_SLEEP_MODE_FLAG, TX_OR);
        	  break;
          case TEST_SENSORS_CMD:
        	  // unit tests for all sensors
        	  tx_event_flags_set(&imu_event_flags_group, IMU_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_set(&bms_event_flags_group, BMS_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_set(&depth_event_flags_group, DEPTH_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_set(&ecg_event_flags_group, ECG_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_set(&audio_event_flags_group, AUDIO_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_set(&light_event_flags_group, LIGHT_UNIT_TEST_FLAG, TX_OR);
        	  tx_event_flags_set(&rtc_event_flags_group, RTC_UNIT_TEST_FLAG, TX_OR);
        	  break;
          case READ_SENSOR_CMD:
        	  tx_event_flags_set(&bms_event_flags_group, BMS_GET_ALL_REG_FLAG, TX_OR);
        	  break;
          case WRITE_SENSOR_CMD:
        	  // imu command only option
        	  if (usbReceiveBuffer[2] == 0) {
        		  // reset imu
        		  if (usbReceiveBuffer[3] == 0) {
                	  tx_event_flags_set(&imu_event_flags_group, IMU_RESET_FLAG, TX_OR);
        		  }
        		  // get samples from imu
        		  else if (usbReceiveBuffer[3] == 1) {
                	  tx_event_flags_set(&imu_event_flags_group, IMU_GET_SAMPLES_FLAG, TX_OR);
        		  }
        		  // double check
        		  else if (usbReceiveBuffer[3] == 2) {
					  tx_event_flags_set(&imu_event_flags_group, IMU_SET_CONFIG_FLAG, TX_OR);
				  }
        	  }
        	  // bms write only option
        	  if (usbReceiveBuffer[2] == 1) {

        	  }
        	  // bms command only option
        	  else if (usbReceiveBuffer[2] == 2) {
        		  // start discharge, stop charge
        		  if (usbReceiveBuffer[3] == 0) {
        			  tx_event_flags_set(&bms_event_flags_group, BMS_STOP_CHARGE_FLAG, TX_OR);
        		  }
        		  // start charge, stop discharge
        		  else if (usbReceiveBuffer[3] == 1) {
        			  tx_event_flags_set(&bms_event_flags_group, BMS_START_CHARGE_FLAG, TX_OR);
        		  }
        	  }
        	  break;
          case SIM_CMD:
        	  // exit simulation mode
        	  if (usbReceiveBuffer[2] == 0) {
        		  tx_event_flags_set(&state_machine_event_flags_group, STATE_EXIT_SIM_FLAG, TX_OR);
        	  }
        	  // enter simulation mode
        	  else if (usbReceiveBuffer[2] == 1) {
        		  SET_SIMULATION_STATE = usbReceiveBuffer[3];
        		  tx_event_flags_set(&state_machine_event_flags_group, STATE_ENTER_SIM_FLAG, TX_OR);
        	  }
          default:
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

/* USER CODE END 1 */
