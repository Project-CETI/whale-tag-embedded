//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "../tests.h"

#include "../tui.h"
#include "../../cetiTagApp/cetiTag.h"

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

TestState test_temperature(FILE *pResultsFile) {
    return TEST_STATE_FAILED;
}

// // Tests that all temperature sensors are +/- 3 degree C of their average value
// TestState test_temperature(void){
//     char input = 0;
//     double garbage;
//     int board_temp_c, battery_temp_c;
//     double temp_c[5];
//     bool sensor_pass[sizeof(temp_c)/sizeof(*temp_c)];

//     do {
//         //read temperatures
//         int result = 0;
//         result |= getPressureTemperature(&garbage, &temp_c[0]);
//         result |= getTemperatures(&board_temp_c, &battery_temp_c);
//         temp_c[1] = (double)board_temp_c;
//         temp_c[2] = (double)battery_temp_c;
//         temp_c[3] = get_cpu_temperature_c();
//         temp_c[4] = get_gpu_temperature_c();
//         if(result != 0){
//             return TEST_STATE_FAILED;
//         }

//         // calculate average temperature
//         double average_temp = 0.0;
//         for(int i = 0; i < sizeof(temp_c)/sizeof(*temp_c); i++){
//             average_temp += temp_c[i] / (sizeof(temp_c)/sizeof(*temp_c));
//         }

//         //check that temperature reads agree +/- 3 deg
//         for(int i = 0; i < sizeof(temp_c)/sizeof(*temp_c); i++){
//             if(!sensor_pass[i]) {
//                 double diff = fabs(average_temp - temp_c[i]);
//                 sensor_pass[i] = (diff <= 3.0);
//             }
//         }


//         //draw results
//         //clear dynamic portion of screen
//         for(int i = 0; i < (1 + sizeof(temp_c)/sizeof(*temp_c)); i++){
//             printf("\e[%d;1H\e[0K", 4 + i);
//         }
//         printf("\e[4;1H");
//         printf("Pressure: %4.1f : %s\n", temp_c[0], sensor_pass[0] ? GREEN(PASS) :  RED(FAIL));
//         printf("Board   : %4.1f : %s\n", temp_c[1], sensor_pass[1] ? GREEN(PASS) :  RED(FAIL));
//         printf("Battery : %4.1f : %s\n", temp_c[2], sensor_pass[2] ? GREEN(PASS) :  RED(FAIL));
//         printf("CPU     : %4.1f : %s\n", temp_c[3], sensor_pass[3] ? GREEN(PASS) :  RED(FAIL));
//         printf("GPU     : %4.1f : %s\n", temp_c[4], sensor_pass[4] ? GREEN(PASS) :  RED(FAIL));
//         printf("Average : %4.1f", average_temp);        
//     } while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0));

//     //print results
//     fprintf(results_file, "Pressure: %s\n", sensor_pass[0] ? PASS :  FAIL);
//     fprintf(results_file, "Board   : %s\n", sensor_pass[1] ? PASS :  FAIL);
//     fprintf(results_file, "Battery : %s\n", sensor_pass[2] ? PASS :  FAIL);
//     fprintf(results_file, "CPU     : %s\n", sensor_pass[3] ? PASS :  FAIL);
//     fprintf(results_file, "GPU     : %s\n", sensor_pass[4] ? PASS :  FAIL);

//     if(input == 27)
//         return TEST_STATE_TERMINATE;


//     bool all_passed = 1;
//     for(int i = 0; i < sizeof(sensor_pass)/sizeof(*sensor_pass); i++){
//         all_passed &= sensor_pass[i];
//     }
//     return (all_passed ? TEST_STATE_PASSED : TEST_STATE_FAILED);
// }
