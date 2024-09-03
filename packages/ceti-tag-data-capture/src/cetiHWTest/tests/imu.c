//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "../tests.h"

#include "../tui.h"
#include "../../cetiTagApp/cetiTag.h"

#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../memory.h"

typedef struct { // euler angles
    double roll;
    double pitch;
    double yaw;
} EulerAngles_f64;

static void __quat_to_euler(EulerAngles_f64 *e, const CetiImuQuatSample *q) {
  double re = ((double) q->real) / (1 << 14);
  double i = ((double) q->i) / (1 << 14);
  double j = ((double) q->j) / (1 << 14);
  double k = ((double) q->k) / (1 << 14);

  double sinr_cosp = 2 * ((re * i) + (j * k));
  double cosr_cosp = 1 - 2 * ((i * i) + (j * j));
  e->pitch = atan2(sinr_cosp, cosr_cosp);

  double sinp = sqrt(1 + 2 * ((re * j) - (i * k)));
  double cosp = sqrt(1 - 2 * ((re * j) - (i * k)));
  e->roll = (2.0 * atan2(sinp, cosp)) - (M_PI / 2.0);

  double siny_cosp = 2 * ((re * k) + (i * j));
  double cosy_cosp = 1 - 2 * ((j * j) + (k * k));
  e->yaw = atan2(siny_cosp, cosy_cosp);
}

// Test of rotation along X, Y and Z
TestState test_imu(FILE *pResultsFile){
    char input = '\0';
    int roll_pass = 0;
    int pitch_pass = 0;
    int yaw_pass = 0;
    int test_index = 0;

    CetiImuQuatSample *shm_quat;
    CetiImuQuatSample *shm_accel;
    CetiImuQuatSample *shm_gyro;
    CetiImuQuatSample *shm_mag;
    sem_t *sem_quat_ready;
    sem_t *sem_accel_ready;
    sem_t *sem_gyro_ready;
    sem_t *sem_mag_ready;

    // === open quaternion shared memory ===
    shm_quat = shm_open_read(IMU_QUAT_SHM_NAME, sizeof(CetiImuQuatSample));
    if(shm_quat == NULL) {
        fprintf(pResultsFile, "[FAIL]: IMU: Failed to open rotation sensor shared memory\n");
        perror("shm_open_read");
        return TEST_STATE_FAILED;
    }
    sem_quat_ready = sem_open(IMU_QUAT_SEM_NAME, O_RDWR, 0444, 0);
    if(sem_quat_ready == SEM_FAILED){
        perror("sem_open");
        munmap(shm_quat, sizeof(CetiImuQuatSample));
        return TEST_STATE_FAILED;
    }


    shm_accel = shm_open_read(IMU_ACCEL_SHM_NAME, sizeof(CetiImuAccelSample));
    if(shm_accel == NULL) {
        fprintf(pResultsFile, "[FAIL]: IMU: Failed to open acceleration shared memory\n");
        perror("shm_open_read");
        return TEST_STATE_FAILED;
    }
    sem_accel_ready = sem_open(IMU_ACCEL_SEM_NAME, O_RDWR, 0444, 0);
    if(sem_accel_ready == SEM_FAILED){
        perror("sem_open");
        munmap(shm_accel, sizeof(CetiImuAccelSample));
        return TEST_STATE_FAILED;
    }


    shm_gyro = shm_open_read(IMU_GYRO_SHM_NAME, sizeof(CetiImuGyroSample));
    if(shm_gyro == NULL) {
        fprintf(pResultsFile, "[FAIL]: IMU: Failed to open gyro shared memory\n");
        perror("shm_open_read");
        return TEST_STATE_FAILED;
    }
    sem_gyro_ready = sem_open(IMU_GYRO_SEM_NAME, O_RDWR, 0444, 0);
    if(sem_gyro_ready == SEM_FAILED){
        perror("sem_open");
        munmap(shm_gyro, sizeof(CetiImuGyroSample));
        return TEST_STATE_FAILED;
    }


    shm_mag = shm_open_read(IMU_MAG_SHM_NAME, sizeof(CetiImuMagSample));
    if(shm_mag == NULL) {
        fprintf(pResultsFile, "[FAIL]: IMU: Failed to open magnetometer shared memory\n");
        perror("shm_open_read");
        return TEST_STATE_FAILED;
    }

    sem_mag_ready = sem_open(IMU_MAG_SEM_NAME, O_RDWR, 0444, 0);
    if(sem_mag_ready == SEM_FAILED){
        perror("sem_open");
        munmap(shm_mag, sizeof(CetiImuMagSample));
        return TEST_STATE_FAILED;
    }

    //get start angle
    printf("Instructions: rotate the tag to meet each green target position below\n\n");

    do{
        //wait for rotation sample
        sem_wait(sem_quat_ready);

        //update test
        EulerAngles_f64 euler_angles;
        __quat_to_euler(&euler_angles, shm_quat);
        
        euler_angles.yaw *= -1.0;

        //update live view
        int width = (tui_get_screen_width() - 13) - 2 ;
        printf("\e[3;11H-180\e[3;%dH0\e[3;%dH180\n", 13 + width/2, tui_get_screen_width() - 2 - 1);
        printf("\e[4;1H\e[0KPitch: %4s\e[4;13H|\e[4;%dH|\e[4;%dH|\n", pitch_pass ? GREEN(PASS) : "",13 + width/2, tui_get_screen_width() - 2);
        printf("\e[5;1H\e[0KYaw  : %4s\e[5;13H|\e[5;%dH|\e[5;%dH|\n", yaw_pass ? GREEN(PASS) : "", 13 + width/2, tui_get_screen_width() - 2);
        printf("\e[6;1H\e[0KRoll : %4s\e[6;13H|\e[6;%dH|\e[6;%dH|\n", roll_pass ? GREEN(PASS) : "", 13 + width/2, tui_get_screen_width() - 2);

        // Accuracy:
        printf("\e[8;1H%-8s%d", "Quat", shm_quat->accuracy);
        sem_wait(sem_accel_ready);
        printf("\e[8;1H%-8s%d", "Accel", shm_accel->accuracy);
        sem_wait(sem_gyro_ready);
        printf("\e[8;1H%-8s%d", "Gryo", shm_gyro->accuracy);
        sem_wait(sem_mag_ready);
        printf("\e[8;1H%-8s%d", "Mag", shm_mag->accuracy);

        //draw reading position
        if(euler_angles.pitch > 0){
            tui_draw_horzontal_bar(euler_angles.pitch, M_PI, 13 + width/2, 4, width/2);
        }
        else if (euler_angles.pitch < 0) {
            tui_draw_inv_horzontal_bar(euler_angles.pitch, -M_PI, 13, 4, width/2);
        }

        if(euler_angles.yaw > 0){
            tui_draw_horzontal_bar(euler_angles.yaw, M_PI, 13 + width/2, 5, width/2);
        }
        else if (euler_angles.yaw < 0) {
            tui_draw_inv_horzontal_bar(euler_angles.yaw, -M_PI, 13, 5, width/2);
        }

        if(euler_angles.roll > 0){
            tui_draw_horzontal_bar(euler_angles.roll, M_PI, 13 + width/2, 6, width/2);
        }
        else if (euler_angles.roll < 0) {
            tui_draw_inv_horzontal_bar(euler_angles.roll, -M_PI, 13, 6, width/2);
        }

        switch(test_index){
            case 0: //-90 pitch
                printf("\e[4;%dH" GREEN("|"), 13 + width/4);
                if(-95.0 <= (euler_angles.pitch*180.0/M_PI) && (euler_angles.pitch*180.0/M_PI) < -85.0){
                    test_index++;
                }
                break;
            case 1: //90 pitch
                printf("\e[4;%dH" GREEN("|"), 13 + (width/4*3));
                if(85.0 <= (euler_angles.pitch*180.0/M_PI) && (euler_angles.pitch*180.0/M_PI) < 95.0){
                    pitch_pass = 1;
                    test_index++;
                }
                break;
            case 2: //-90 yaw
                printf("\e[5;%dH" GREEN("|"), 13 + width/4);
                if(-95.0 <= (euler_angles.yaw*180.0/M_PI) && (euler_angles.yaw*180.0/M_PI) < -85.0){
                    test_index++;
                }
                break;
            case 3: //90 yaw
                printf("\e[5;%dH" GREEN("|"), 13 + (width/4*3));
                if(85.0 <= (euler_angles.yaw*180.0/M_PI) && (euler_angles.yaw*180.0/M_PI) < 95.0){
                    yaw_pass = 1;
                    test_index++;
                }
                break;
            case 4: //-90 roll
                printf("\e[6;%dH" GREEN("|"), 13 + width/4);
                if(-95.0 <= (euler_angles.roll*180.0/M_PI) && (euler_angles.roll*180.0/M_PI) < -85.0){
                    test_index++;
                }
                break;
            case 5: //90 roll
                printf("\e[6;%dH" GREEN("|"), 13 + (width/4*3));
                if(85.0 <= (euler_angles.roll*180.0/M_PI) && (euler_angles.roll*180.0/M_PI) < 95.0){
                    test_index++;
                    roll_pass = 1;
                }
                break;
            default:
                break;
        }

        fflush(stdout);
    } while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0));

    //record results
    fprintf(pResultsFile, "[%s]: roll\n", roll_pass ? "PASS" : "FAIL");
    fprintf(pResultsFile, "[%s]: pitch\n", pitch_pass ? "PASS" : "FAIL");
    fprintf(pResultsFile, "[%s]: yaw\n", yaw_pass ? "PASS" : "FAIL");

    sem_close(sem_quat_ready);
    munmap(shm_quat, sizeof(CetiImuQuatSample));
    
    return (input == 27) ? TEST_STATE_TERMINATE
         : (roll_pass && pitch_pass && yaw_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
    return TEST_STATE_FAILED;
}
