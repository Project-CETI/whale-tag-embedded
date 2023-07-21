/*
 * tasks.c
 *
 *  Created on: Jul 6, 2023
 *      Author: Kaveet
 */

#ifndef SRC_LIB_SRC_THREADS_C_
#define SRC_LIB_SRC_THREADS_C_

#include <Lib Inc/threads.h>
#include <stdint.h>

Thread_HandleTypeDef threads[NUM_THREADS];


void threadListInit(){

	for (Thread index = 0; index < NUM_THREADS; index++){

		//Attach our config info (the ones statically defined in the header file)
		threads[index].config = threadConfigList[index];

		//Allocate thread stack space
		threads[index].thread_stack_start = malloc(threads[index].config.thread_stack_size);
	}
}
#endif /* SRC_LIB_SRC_TASKS_C_ */
