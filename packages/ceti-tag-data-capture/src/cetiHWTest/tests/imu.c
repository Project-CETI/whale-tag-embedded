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

// // Test of rotation along X, Y and Z
TestState test_imu(FILE *pResultsFile){
//     char input = '\0';
//     int roll_pass = 0;
//     int pitch_pass = 0;
//     int yaw_pass = 0;
//     int test_index = 0;

//     //initialize IMU
//     if(setupIMU(IMU_QUAT_ENABLED) < 0) {
//         printf(RED(FAIL) " IMU Communication Failure.\n");
//         while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)){ ; }
//         bbI2CClose(IMU_BB_I2C_SDA);
//         return TEST_STATE_FAILED; //imu communication error
//     }

//     //get start angle
//     printf("Instructions: rotate the tag to meet each green target position below\n\n");

//     while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)){
//         EulerAngles_f64 euler_angles = {};
        
//         //update test
//         int report_id_updated = imu_get_euler_angles(&euler_angles);
//         if(report_id_updated == -1) { // no data received or wrong data type
//             continue; // Note that we are streaming 4 reports at 50 Hz, so we expect data to be available at about 200 Hz
//         }

//         euler_angles.yaw *= -1.0;

//         //update live view
//         int width = (tui_get_screen_width() - 13) - 2 ;
//         printf("\e[3;11H-180\e[3;%dH0\e[3;%dH180\n", 13 + width/2, tui_get_screen_width() - 2 - 1);
//         printf("\e[4;1H\e[0KPitch: %4s\e[4;13H|\e[4;%dH|\e[4;%dH|\n", pitch_pass ? GREEN(PASS) : "",13 + width/2, tui_get_screen_width() - 2);
//         printf("\e[5;1H\e[0KYaw  : %4s\e[5;13H|\e[5;%dH|\e[5;%dH|\n", yaw_pass ? GREEN(PASS) : "", 13 + width/2, tui_get_screen_width() - 2);
//         printf("\e[6;1H\e[0KRoll : %4s\e[6;13H|\e[6;%dH|\e[6;%dH|\n", roll_pass ? GREEN(PASS) : "", 13 + width/2, tui_get_screen_width() - 2);

//         //draw reading position
//         if(euler_angles.pitch > 0){
//             tui_draw_horzontal_bar(euler_angles.pitch, M_PI, 13 + width/2, 4, width/2);
//         }
//         else if (euler_angles.pitch < 0) {
//             tui_draw_inv_horzontal_bar(euler_angles.pitch, -M_PI, 13, 4, width/2);
//         }

//         if(euler_angles.yaw > 0){
//             tui_draw_horzontal_bar(euler_angles.yaw, M_PI, 13 + width/2, 5, width/2);
//         }
//         else if (euler_angles.yaw < 0) {
//             tui_draw_inv_horzontal_bar(euler_angles.yaw, -M_PI, 13, 5, width/2);
//         }

//         if(euler_angles.roll > 0){
//             tui_draw_horzontal_bar(euler_angles.roll, M_PI, 13 + width/2, 6, width/2);
//         }
//         else if (euler_angles.roll < 0) {
//             tui_draw_inv_horzontal_bar(euler_angles.roll, -M_PI, 13, 6, width/2);
//         }

//         switch(test_index){
//             case 0: //-90 pitch
//                 printf("\e[4;%dH" GREEN("|"), 13 + width/4);
//                 if(-95.0 <= (euler_angles.pitch*180.0/M_PI) && (euler_angles.pitch*180.0/M_PI) < -85.0){
//                     test_index++;
//                 }
//                 break;
//             case 1: //90 pitch
//                 printf("\e[4;%dH" GREEN("|"), 13 + (width/4*3));
//                 if(85.0 <= (euler_angles.pitch*180.0/M_PI) && (euler_angles.pitch*180.0/M_PI) < 95.0){
//                     pitch_pass = 1;
//                     test_index++;
//                 }
//                 break;
//             case 2: //-90 yaw
//                 printf("\e[5;%dH" GREEN("|"), 13 + width/4);
//                 if(-95.0 <= (euler_angles.yaw*180.0/M_PI) && (euler_angles.yaw*180.0/M_PI) < -85.0){
//                     test_index++;
//                 }
//                 break;
//             case 3: //90 yaw
//                 printf("\e[5;%dH" GREEN("|"), 13 + (width/4*3));
//                 if(85.0 <= (euler_angles.yaw*180.0/M_PI) && (euler_angles.yaw*180.0/M_PI) < 95.0){
//                     yaw_pass = 1;
//                     test_index++;
//                 }
//                 break;
//             case 4: //-90 roll
//                 printf("\e[6;%dH" GREEN("|"), 13 + width/4);
//                 if(-95.0 <= (euler_angles.roll*180.0/M_PI) && (euler_angles.roll*180.0/M_PI) < -85.0){
//                     test_index++;
//                 }
//                 break;
//             case 5: //90 roll
//                 printf("\e[6;%dH" GREEN("|"), 13 + (width/4*3));
//                 if(85.0 <= (euler_angles.roll*180.0/M_PI) && (euler_angles.roll*180.0/M_PI) < 95.0){
//                     test_index++;
//                     roll_pass = 1;
//                 }
//                 break;
//             default:
//                 break;
//         }

//         fflush(stdout);
//     }

//     //record results
//     fprintf(results_file, "[%s]: roll\n", roll_pass ? "PASS" : "FAIL");
//     fprintf(results_file, "[%s]: pitch\n", pitch_pass ? "PASS" : "FAIL");
//     fprintf(results_file, "[%s]: yaw\n", yaw_pass ? "PASS" : "FAIL");

//     resetIMU();
//     bbI2CClose(IMU_BB_I2C_SDA);
    
//     return (input == 27) ? TEST_STATE_TERMINATE
//          : (roll_pass && pitch_pass && yaw_pass) ? TEST_STATE_PASSED
//          : TEST_STATE_FAILED;
    return TEST_STATE_FAILED;
}