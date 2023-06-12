/*
 * UnitTests.h
 *
 *  Created on: Feb. 9, 2023
 *      Author: amjad
 */

#ifndef INC_UNITTESTS_H_
#define INC_UNITTESTS_H_

#include <Sensor Inc/ad7768.h>
#include <Sensor Inc/KellerDepth.h>
#include <Sensor Inc/LightSensor.h>
#include <Sensor Inc/BNO08x.h>
#include <Sensor Inc/ECG.h>


// Steps for writing a unit test
// Include the necessary header files to write the unit test
// Define expected value, and tolerance for the sensors output
// Make a prototype of the unit test and write the function in the src file

// Atmospheric Pressure and ambient temperature
#define ATM_PRES	0.0f	// sensor reads pressure in reference to 1.0 atm
#define AMB_TEMP	21.0f

// Pressure and temperature tolerance
#define PRES_TOL	0.2f
#define TEMP_TOL	2.0f

// TODO: Need to change to correct values, after finding lux formula
// Average wood lab room lux
#define AVG_LUX		300
#define AVG_IR_LUX	200

// Lux tolerance
#define LUX_TOL		15

void Keller_UT(Keller_HandleTypedef *keller_sensor);
void Light_UT(LightSensorHandleTypedef *light_sensor);
void IMU_UT(IMU_HandleTypeDef* imu);
void AD7768_UT(ad7768_dev *adc);
bool SDcard_UT(void);
void ECG_UT(ECG_HandleTypeDef *ecg);

#endif /* INC_UNITTESTS_H_ */
