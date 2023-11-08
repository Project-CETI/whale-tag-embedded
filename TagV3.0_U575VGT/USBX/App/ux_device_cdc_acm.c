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

  ULONG actual_length;
  UX_SLAVE_DEVICE *device;

  UX_PARAMETER_NOT_USED(thread_input);

  device = &_ux_system_slave->ux_system_slave_device;

  while (1) {
    if ((device->ux_slave_device_state == UX_DEVICE_CONFIGURED) && (cdc_acm != UX_NULL)) {
      #ifndef UX_DEVICE_CLASS_CDC_ACM_TRANSMISSION_DISABLE

      cdc_acm -> ux_slave_class_cdc_acm_transmission_status = UX_FALSE;

      #endif

      // read received data in blocking mode
      ux_device_class_cdc_acm_read(cdc_acm, (UCHAR *) &usbReceiveBuf[0], 2, &actual_length);

      if (actual_length != 0 && usbReceiveBuf[0] == START_CHAR) {
        if (usbReceiveBuf[1] == SLEEP_CMD) {
		  //if (ux_device_class_cdc_acm_write(cdc_acm, (UCHAR *) &usbReceiveBuf[1], 1, &actual_length) != UX_SUCCESS) {
		  //  Error_Handler();
		  //}
          tx_event_flags_set(&state_machine_event_flags_group, STATE_SLEEP_MODE_FLAG, TX_OR);
        }
        else if (usbReceiveBuf[1] == WAKE_CMD) {
		  //if (ux_device_class_cdc_acm_write(cdc_acm, (UCHAR *) &usbReceiveBuf[1], 1, &actual_length) != UX_SUCCESS) {
		  //  Error_Handler();
		  //}
		  tx_event_flags_set(&state_machine_event_flags_group, STATE_EXIT_SLEEP_MODE_FLAG, TX_OR);
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
