/*
 * AprsTransmit.c
 *
 *  Created on: May 26, 2023
 *      Author: Kaveet
 */

#include "Recovery Inc/AprsTransmit.h"
#include "constants.h"
#include "main.h"

//Private functions
static void calcSineValues();

//Private variables
static bool byteCompleteFlag;
static bool is1200Hz = false;

//Extern variables
extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim2;

uint32_t dac_input[APRS_TRANSMIT_NUM_SINE_SAMPLES];

uint8_t bitStuffSamples[255] = {0};
uint8_t sampleIndex = 0;

bool gpioFlag = true;

bool aprs_transmit_send_data(uint8_t * packet_data, uint16_t packet_length){

	calcSineValues();

	//Timer variables for transmitting bits
	TX_TIMER bit_timer;

	//Start our DAC and our timer to trigger the conversion edges
	HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, dac_input, 100, DAC_ALIGN_8B_R);
	HAL_TIM_Base_Start(&htim2);

	//Loop through each byte
	for (int byte_index = 0; byte_index < packet_length; byte_index++){

		//Set complete flag to false so we can poll it later
		byteCompleteFlag = false;

		//Create a timer to control the transmission of each bit. We pass in the current byte as an input to the timer so it can iterate over it bit by bit.
		tx_timer_create(&bit_timer, "APRS Transmit Bit Timer", aprs_transmit_bit_timer_entry, packet_data[byte_index], APRS_TRANSMIT_BIT_TIME, APRS_TRANSMIT_BIT_TIME, TX_AUTO_ACTIVATE);

		//Poll for completion of the byte
		while (!byteCompleteFlag);

		//Delete the timer so we can recreate it later with the next byte as an input
		tx_timer_delete(&bit_timer);
	}

	//Stop DAC and timer
	HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
	HAL_TIM_Base_Stop(&htim2);

	return true;
}

void aprs_transmit_bit_timer_entry(ULONG bit_timer_input){

	//static variable to keep track of our bit index
	static uint8_t bit_index = 0;
	static uint8_t bit_stuff_counter = 0;
	static bool isStuffedBit = false;

	//Current byte we will iterate over
	uint8_t current_byte = (uint8_t) bit_timer_input;

	if (bit_stuff_counter >= 5){
		isStuffedBit = true;

	}

	//Check if the current bit is 0
	if (!aprs_transmit_read_bit(current_byte, bit_index) || isStuffedBit){

		if (isStuffedBit){
			bitStuffSamples[sampleIndex] = 2;
		}else{
			bitStuffSamples[sampleIndex] = 0;
		}
		//Since the bit is 0, switch the frequency
		uint8_t newPeriod = (is1200Hz) ? (APRS_TRANSMIT_PERIOD_2400HZ) : (APRS_TRANSMIT_PERIOD_1200HZ);
		is1200Hz = !is1200Hz;

		//Use fake init function to re-initialize the timer with a new period
		MX_TIM2_Fake_Init(newPeriod);
		bit_stuff_counter = 0;



	} else {
		bitStuffSamples[sampleIndex] = 1;
		bit_stuff_counter++;
	}


	if (!isStuffedBit){
		//increment bit index
		bit_index++;
		gpioFlag = !gpioFlag;
		HAL_GPIO_WritePin(IMU_WAKE_GPIO_Port, IMU_WAKE_Pin, gpioFlag);
	}else {
		isStuffedBit = false;
	}

	//If we've iterated through all bits, set flag and reset index counter
	if (bit_index >= BITS_PER_BYTE){
		byteCompleteFlag = true;
		bit_index = 0;
	}

	sampleIndex++;
}

//Calculates an array of digital values to pass into the DAC in order to generate a sine wave.
static void calcSineValues(){

	for (uint8_t i = 0; i < APRS_TRANSMIT_NUM_SINE_SAMPLES; i++){

		//Formula taken from STM32 documentation online on sine wave generation.
		//Generates a sine wave with a min of 0V and a max of the reference voltage.
		dac_input[i] = ((sin(i * 2 * PI/APRS_TRANSMIT_NUM_SINE_SAMPLES) + 1) * (43)) + 170;
	}
}
