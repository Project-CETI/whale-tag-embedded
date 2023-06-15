/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
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
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "Recovery Inc/AprsTransmit.h"
#include "Recovery Inc/VHF.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define THREAD_STACK_SIZE 2048
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
TX_THREAD test_thread;
uint8_t thread_stack[THREAD_STACK_SIZE];
extern UART_HandleTypeDef huart4;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void test_thread_entry(ULONG thread_input);
void timer_entry(ULONG timer_input);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  VOID * pointer = &thread_stack;
  // Allocate memory pool
  TX_BYTE_POOL* byte_pool = (TX_BYTE_POOL*) memory_ptr;
  ret = tx_byte_allocate(byte_pool, &pointer, THREAD_STACK_SIZE, TX_NO_WAIT);
  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */
  tx_thread_create(&test_thread, "Test_Thread", test_thread_entry, 0x1234, thread_stack, THREAD_STACK_SIZE, 3, 3, TX_NO_TIME_SLICE, TX_AUTO_START);
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  MX_ThreadX_Init
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
void test_thread_entry(ULONG thread_input){

	//test
	//uint8_t data[224] = {0x7e,0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e,0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x82, 0xa0 , 0xa4 , 0xa6 , 0x40 , 0x40 , 0x60 , 0x94 , 0x6e , 0x6a , 0xb2 , 0x40 , 0x40 , 0x62 , 0xae , 0x92 , 0x88 , 0x8a , 0x64 , 0x40 , 0x65 , 0x3 , 0xf0 , 0x21 , 0x34 , 0x32 , 0x32 , 0x31 , 0x2e , 0x38 , 0x31 , 0x4e , 0x31 , 0x30 , 0x37 , 0x31 , 0x30 , 0x37 , 0x2e , 0x35 , 0x35 , 0x57 , 0x73 , 0x33 , 0x36 , 0x30 , 0x2f , 0x30 , 0x30 , 0x30 , 0x42 , 0x75 , 0x69 , 0x6c , 0x64 , 0x20 , 0x44 , 0x65 , 0x6d , 0x6f , 0x6e , 0x73 , 0x74 , 0x72 , 0x61 , 0x74 , 0x69 , 0x6f , 0x6e , 0xdc , 0x1a , 0x7e , 0x7e , 0x7e };
	//volatile HAL_StatusTypeDef ret = initialize_vhf(huart4, false);


	//uint8_t data[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

	uint8_t data[3] = {0x7E, 0x7E, 0x7E};


	/* Enter forever loop */
	while (1){
		set_ptt(true);
		aprs_transmit_send_data(data, 1);
		set_ptt(false);
		HAL_Delay(1000);
	}

}

/* USER CODE END 1 */
