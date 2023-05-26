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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define THREAD_STACK_SIZE 1024
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
TX_THREAD test_thread;
uint8_t thread_stack[THREAD_STACK_SIZE];
uint32_t test_thread_counter = 0;
TX_TIMER timer_ptr;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;

bool is2400Hz = true;
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

	//Create test timer
	//tx_timer_create(&timer_ptr, "My Timer", timer_entry, 0x1234, 1000, 1000, TX_AUTO_ACTIVATE);
	/* Enter forever loop */

	uint8_t data[2] = {0x55, 0xE1};
	aprs_transmit_send_data(data, 2);
	while (1){

	}
}

/* USER CODE END 1 */
