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
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

TestState test_temperature(FILE *pResultsFile) {
    char input = '\0';

    CetiBatterySample *shm_battery = NULL;
    CetiPressureSample *shm_pressure = NULL;
    sem_t *sem_bms_ready;
    sem_t *sem_pressure_ready;

    double temp_c[3];
    int32_t sensor_pass[sizeof(temp_c)/sizeof(*temp_c)] = {};


    // === open batteries shared memory ===
    int shm_fd = shm_open(BATTERY_SHM_NAME, O_RDWR, 0444);
    if (shm_fd < 0) {
        fprintf(pResultsFile, "[FAIL]: BMS: Failed to open shared memory\n");
        perror("shm_open");
        return TEST_STATE_FAILED;
    }
    // size to sample size
    if (ftruncate(shm_fd, sizeof(CetiBatterySample))){
        fprintf(pResultsFile, "[FAIL]: BMS: Failed to size shared memory\n");
        perror("ftruncate");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }
    // memory map address
    shm_battery = mmap(NULL, sizeof(CetiBatterySample), PROT_READ , MAP_SHARED, shm_fd, 0);
    if(shm_battery == MAP_FAILED){
        perror("mmap");
        fprintf(pResultsFile, "[FAIL]: BMS: Failed to map shared memory\n");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }
    close(shm_fd);

    sem_bms_ready = sem_open(BATTERY_SEM_NAME, O_RDWR, 0444, 0);
    if(sem_bms_ready == SEM_FAILED){
        perror("sem_open");
        munmap(shm_battery, sizeof(CetiBatterySample));
        return TEST_STATE_FAILED;
    }

    // === open pressure shared memory ===
    shm_fd = shm_open(PRESSURE_SHM_NAME, O_RDWR, 0444);
    if (shm_fd < 0) {
        perror("shm_open");
        fprintf(pResultsFile, "[FAIL]: Pressure: Failed to open shared memory\n");
        sem_close(sem_bms_ready);
        munmap(shm_battery, sizeof(CetiBatterySample));
        return TEST_STATE_FAILED;
    }

    // size to sample size
    if (ftruncate(shm_fd, sizeof(CetiPressureSample))){
        perror("shm_fd");
        fprintf(pResultsFile, "[FAIL]: Pressure: Failed to size shared memory\n");
        close(shm_fd);
        sem_close(sem_bms_ready);
        munmap(shm_battery, sizeof(CetiBatterySample));
        return TEST_STATE_FAILED;
    }

    // memory map address
    shm_pressure = mmap(NULL, sizeof(CetiPressureSample), PROT_READ, MAP_SHARED, shm_fd, 0);
    if(shm_pressure == MAP_FAILED){
        perror("mmap");
        fprintf(pResultsFile, "[FAIL]: Pressure: Failed to map shared memory\n");
        close(shm_fd);
        sem_close(sem_bms_ready);
        munmap(shm_battery, sizeof(CetiBatterySample));
        return TEST_STATE_FAILED;
    }

    // open pressure shared memory object
    sem_pressure_ready =  sem_open(PRESSURE_SEM_NAME, O_RDWR, 0644, 0);
    if(sem_pressure_ready == SEM_FAILED){
        perror("sem_open");
        fprintf(pResultsFile, "[FAIL]: Pressure: Failed to open\n");
        munmap(shm_pressure, sizeof(CetiPressureSample));
        sem_close(sem_bms_ready);
        munmap(shm_battery, sizeof(CetiBatterySample));
        return TEST_STATE_FAILED;
    }

    do{
        sem_wait(sem_pressure_ready);
        if (shm_pressure->error){
            sem_close(sem_pressure_ready);
            munmap(shm_pressure, sizeof(CetiPressureSample));
            sem_close(sem_bms_ready);
            munmap(shm_battery, sizeof(CetiBatterySample));

            return TEST_STATE_FAILED;
        }
        temp_c[0] = shm_pressure->temperature_c;

        sem_wait(sem_bms_ready);
        temp_c[1] = shm_battery->cell_temperature_c[0];
        temp_c[2] = shm_battery->cell_temperature_c[1];
        // temp_c[3] = get_cpu_temperature_c();
        // temp_c[4] = get_gpu_temperature_c();


        // calculate average temperature
        double average_temp = 0.0;
        for(int i = 0; i < sizeof(temp_c)/sizeof(*temp_c); i++){
            average_temp += temp_c[i] / (sizeof(temp_c)/sizeof(*temp_c));
        }

        //check that temperature reads agree +/- 3 deg
        for(int i = 0; i < sizeof(temp_c)/sizeof(*temp_c); i++){
            if(!sensor_pass[i]) {
                double diff = fabs(average_temp - temp_c[i]);
                sensor_pass[i] = (diff <= 3.0);
            }
        }


        //draw results
        //clear dynamic portion of screen
        for(int i = 0; i < (1 + sizeof(temp_c)/sizeof(*temp_c)); i++){
            printf("\e[%d;1H\e[0K", 4 + i);
        }
        printf("\e[4;1H");
        printf("Pressure: %4.1f : %s\n", temp_c[0], sensor_pass[0] ? GREEN(PASS) :  RED(FAIL));
        printf("Board   : %4.1f : %s\n", temp_c[1], sensor_pass[1] ? GREEN(PASS) :  RED(FAIL));
        printf("Battery : %4.1f : %s\n", temp_c[2], sensor_pass[2] ? GREEN(PASS) :  RED(FAIL));


    }while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0));

    //print results
    fprintf(pResultsFile, "Pressure: %s\n", sensor_pass[0] ? PASS :  FAIL);
    fprintf(pResultsFile, "Board   : %s\n", sensor_pass[1] ? PASS :  FAIL);
    fprintf(pResultsFile, "Battery : %s\n", sensor_pass[2] ? PASS :  FAIL);
    // fprintf(pResultsFile, "CPU     : %s\n", sensor_pass[3] ? PASS :  FAIL);
    // fprintf(pResultsFile, "GPU     : %s\n", sensor_pass[4] ? PASS :  FAIL);

    sem_close(sem_pressure_ready);
    munmap(shm_pressure, sizeof(CetiPressureSample));
    sem_close(sem_bms_ready);
    munmap(shm_battery, sizeof(CetiBatterySample));

    if(input == 27)
        return TEST_STATE_TERMINATE;


    int all_passed = 1;
    for(int i = 0; i < sizeof(sensor_pass)/sizeof(*sensor_pass); i++){
        all_passed &= sensor_pass[i];
    }
    return (all_passed ? TEST_STATE_PASSED : TEST_STATE_FAILED);

}
