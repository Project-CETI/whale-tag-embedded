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

TestState test_batteries(FILE *pResultsFile) {
    const double cell_min_v = 3.1;
    const double cell_max_v = 4.2;
    const double cell_balance_limit_v = 0.05;
    char input = '\0';
    int cell1_pass = 0;
    int cell2_pass = 0;
    int balance_pass = 0;

    CetiBatterySample *shm_battery;
    sem_t *sem_bms_ready;

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

    //perform test
    do {
        //get sample
        sem_wait(sem_bms_ready);
        if (shm_battery->error != 0) {
            fprintf(pResultsFile, "[FAIL]: BMS: Device error\n");
            sem_close(sem_bms_ready);
            munmap(shm_battery, sizeof(CetiBatterySample));
            return TEST_STATE_FAILED;
        }

        //analyze sample
        cell1_pass = ((cell_min_v < shm_battery->cell_voltage_v[0]) && (shm_battery->cell_voltage_v[0] < cell_max_v));
        cell2_pass = ((cell_min_v < shm_battery->cell_voltage_v[1]) && (shm_battery->cell_voltage_v[1] < cell_max_v));
        double balance = fabs(shm_battery->cell_voltage_v[0] - shm_battery->cell_voltage_v[1]);
        balance_pass = (balance < cell_balance_limit_v);

        //display results
        printf("Cell 1 (%4.2f V): %s\n", shm_battery->cell_voltage_v[0], cell1_pass ? GREEN(PASS) : RED(FAIL));
        printf("Cell 2 (%4.2f V): %s\n", shm_battery->cell_voltage_v[1], cell2_pass ? GREEN(PASS) : RED(FAIL));
        printf("Cell diff (%3.0f mV): %s\n", 1000*balance, balance_pass ? GREEN(PASS) : RED(FAIL));

    }while ((read(STDIN_FILENO, &input, 1) != 1) && (input == 0));
    
    //record results
    fprintf(pResultsFile, "[%s]: Cell 1 (%4.2f V)\n", cell1_pass ? "PASS" : "FAIL", shm_battery->cell_voltage_v[0]);
    fprintf(pResultsFile, "[%s]: Cell 2 (%4.2f V)\n", cell2_pass ? "PASS" : "FAIL", shm_battery->cell_voltage_v[1]);
    fprintf(pResultsFile, "[%s]: Balance (%4.2f mV)\n", balance_pass ? "PASS" : "FAIL", fabs(shm_battery->cell_voltage_v[0] - shm_battery->cell_voltage_v[1]));

    sem_close(sem_bms_ready);
    munmap(shm_battery, sizeof(CetiBatterySample));

    if(input == 27)
        return TEST_STATE_TERMINATE;
    
    if(cell1_pass && cell2_pass && balance_pass){
        return TEST_STATE_PASSED;
    }

    return TEST_STATE_FAILED;
}
