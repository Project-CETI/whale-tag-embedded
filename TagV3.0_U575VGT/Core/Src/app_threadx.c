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
#include "Recovery Inc/Aprs.h"
#include "Sensor Inc/ECG.h"
#include "Lib Inc/threads.h"
#include "Sensor Inc/BNO08x.h"
#include "Sensor Inc/audio.h"
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
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

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

  //Initialize thread list so we can create threads
  threadListInit();

  //loop through each thread in the list, allocate the memory and create the stack
  for (uint8_t index = 0; index < NUM_THREADS; index++){

	  VOID * pointer = threads[index].thread_stack_start;
  // Allocate memory pool
	  TX_BYTE_POOL* byte_pool = (TX_BYTE_POOL*) memory_ptr;
  	  ret = tx_byte_allocate(byte_pool, &pointer, threads[index].config.thread_stack_size, TX_NO_WAIT);

  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */
  	  tx_thread_create(
  			  &threads[index].thread,
			  threads[index].config.thread_name,
			  threads[index].config.thread_entry_function,
			  threads[index].config.thread_input,
			  threads[index].thread_stack_start,
			  threads[index].config.thread_stack_size,
			  threads[index].config.priority,
			  threads[index].config.preempt_threshold,
			  threads[index].config.timeslice,
			  threads[index].config.start);
  }
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

/* USER CODE END 1 */
