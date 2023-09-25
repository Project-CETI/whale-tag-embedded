/*
 * UnitTests.c
 *
 *  Created on: Feb. 9, 2023
 *      Author: amjad
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "UnitTests.h"
#include "app_filex.h"

#define UT_ASSERT( expr) \
    if(expr){\
        printf("Ok\r\n");\
    } else {\
        printf("!!!Err!!\r\n");\
        return false;\
    }

/*
void Keller_UT(Keller_HandleTypedef *keller_sensor){
	Keller_get_data(keller_sensor);
	printf("Keller Sensor Unit Test\r\n");

	if(fabs(keller_sensor->pressure - ATM_PRES) > PRES_TOL){
		printf("\tPressure Failed: %.2fatm (1 atm ref)\r\n", keller_sensor->pressure);
	}
	else{
		printf("\tPressure Passed: %.2fatm (1 atm ref)\r\n", keller_sensor->pressure);
	}

	if(fabs(keller_sensor->temperature - AMB_TEMP) > TEMP_TOL){
		printf("\tTemperature Failed: %.2fC\r\n", keller_sensor->temperature);
	}
	else{
		printf("\tTemperature Passed: %.2fC\r\n", keller_sensor->temperature);
	}
}
*/

void Light_UT(LightSensorHandleTypedef *light_sensor){
	// ALS Integration Time is the measurement time for each ALS cycle
	// Set a delay with the maximum integration time to ensure new data is read
	HAL_Delay(350);
	LightSensor_get_data(light_sensor);
	printf("Light (ALS) Sensor Unit Test:\r\n");

	ALSPartIDRegister part_info;
	ALSManufacIDRegister manu_info;
	LightSensor_get_part_id(light_sensor, &part_info);
	LightSensor_get_manufacturer(light_sensor, &manu_info);
	if((manu_info != ALS_MANUFAC_ID) || (part_info.part_number_id != ALS_PART_ID) || (part_info.revision_id != ALS_REVISION_ID)){
	    printf("\tPart ID Failed:\r\n\
	            \t\tExpected: {manu: 0x%x, part: 0x%x, rev: 0x%x}\r\n\
	            \t\tRecieved: {manu: 0x%x, part: 0x%x, rev: 0x%x}\r\n", ALS_MANUFAC_ID, ALS_PART_ID, ALS_REVISION_ID, \
	            manu_info, part_info.part_number_id, part_info.revision_id
	    );
	}
	else{
	    printf("\tPart ID Passed\r\n");
	}
	if(abs(light_sensor->data.visible - AVG_LUX) > LUX_TOL){
		printf("\tVisible Failed: %d lux\r\n", light_sensor->data.visible);
	}
	else{
		printf("\tVisible Passed: %d lux\r\n", light_sensor->data.visible);
	}

	if(abs(light_sensor->data.infrared - AVG_IR_LUX) > LUX_TOL){
		printf("\tInfrared Failed: %d lux\r\n", light_sensor->data.infrared);
	}
	else{
		printf("\tInfrared Passed: %d lux\r\n", light_sensor->data.infrared);
	}
}

void AD7768_UT(ad7768_dev *adc){
	uint8_t rev_id;
	printf("ADC (hydrophone) Unit Test:\r\n");
	ad7768_get_revision_id(adc, &rev_id);
	if(rev_id != 6){
		printf("\tRevision Test Failed: %d\r\n", rev_id);
	}
	else{
		printf("\tRevision Test Passed\r\n");
	}	
}


bool SDcard_UT(void){
	unsigned int res;
	ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);
	FX_MEDIA        sdio_disk = {}; // init to remove dangling pointers
	FX_FILE         ut_file = {};
	uint8_t         test_buffer[] = {0,1,2,3, 0xDE, 0xAD, 0xBE, 0xEF};
	uint8_t         res_buffer[sizeof(test_buffer)];
	ULONG           read_size;

	printf("SD Card Test:\r\n");
	
    printf("\tMedia mount: ");
    UT_ASSERT(fx_media_open(&sdio_disk, "", fx_stm32_sd_driver, (VOID *)FX_NULL, (VOID *) fx_sd_media_memory, sizeof(fx_sd_media_memory)) == FX_SUCCESS);

	printf("\tFile Create: ");
	res = fx_file_create(&sdio_disk, "UnitTestFile.txt");
	UT_ASSERT((res == FX_SUCCESS) || (res == FX_ALREADY_CREATED));

    printf("\tFile open (write): ");
	UT_ASSERT(fx_file_open(&sdio_disk, &ut_file, "UnitTestFile.txt", FX_OPEN_FOR_WRITE) ==	FX_SUCCESS);

    printf("\tFile write: ");
    UT_ASSERT(fx_file_write(&ut_file, test_buffer, sizeof(test_buffer)) ==  FX_SUCCESS);

	printf("\tFile close: ");
	UT_ASSERT(fx_file_close(&ut_file) == FX_SUCCESS );

	printf("\tFile open (read): ");
    UT_ASSERT(fx_file_open(&sdio_disk, &ut_file, "UnitTestFile.txt", FX_OPEN_FOR_READ) ==  FX_SUCCESS);

    printf("\tFile read: ");
    res = fx_file_read(&ut_file, res_buffer, sizeof(test_buffer), &read_size);
    UT_ASSERT((res == FX_SUCCESS) && !memcmp(res_buffer, test_buffer, sizeof(test_buffer)));

    printf("\tFile close: ");
    UT_ASSERT(fx_file_close(&ut_file) == FX_SUCCESS );

    printf("\tFile delete: ");
    UT_ASSERT( fx_file_delete(&sdio_disk, "UnitTestFile.txt") == FX_SUCCESS );

	printf("\tMedia unmount: ");
	UT_ASSERT(fx_media_close(&sdio_disk) == FX_SUCCESS );

	return true;
}

// Function for printing to SWV
int _write(int file, char *ptr, int len) {
	int DataIdx;
	for (DataIdx = 0; DataIdx < len; DataIdx++) {
		ITM_SendChar(*ptr++);
	}
	return len;
}
