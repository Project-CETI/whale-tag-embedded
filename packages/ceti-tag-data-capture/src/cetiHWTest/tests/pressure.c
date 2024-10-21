//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "../tests.h"

#include "../../cetiTagApp/cetiTag.h"
#include "../tui.h"

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

TestState test_pressure(FILE *pResultsFile) {
    CetiPressureSample *pressure_data = NULL;
    sem_t *pressure_data_ready;
    const double pressure_target = 2.0;
    int pressure_pass = 0;
    char input = 0;

    // open pressure shared memory object
    // open/create ipc file
    int shm_fd = shm_open(PRESSURE_SHM_NAME, O_RDWR, 0644);
    if (shm_fd < 0) {
        perror("shm_open");
        fprintf(pResultsFile, "[FAIL]: Pressure: Failed to open shared memory\n");
        return TEST_STATE_FAILED;
    }

    // size to sample size
    if (ftruncate(shm_fd, sizeof(CetiPressureSample))) {
        perror("shm_fd");
        fprintf(pResultsFile, "[FAIL]: Pressure: Failed to size shared memory\n");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }

    // memory map address
    pressure_data = mmap(NULL, sizeof(CetiPressureSample), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (pressure_data == MAP_FAILED) {
        perror("mmap");
        fprintf(pResultsFile, "[FAIL]: Pressure: Failed to map shared memory\n");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }

    close(shm_fd);

    // open pressure shared memory object
    pressure_data_ready = sem_open(PRESSURE_SEM_NAME, O_RDWR, 0644, 0);
    if (pressure_data_ready == SEM_FAILED) {
        perror("sem_open");
        fprintf(pResultsFile, "[FAIL]: Pressure: Failed to open\n");
        munmap(pressure_data, sizeof(CetiPressureSample));
        return TEST_STATE_FAILED;
    }

    printf("Instructions: Use a syringe to apply pressure to the tag's depth sensor\n");
    do {
        // int result = getPressureTemperature(&pressure_bar, &temperature_c);
        sem_wait(pressure_data_ready);
        if (pressure_data->error != 0) {
            sem_close(pressure_data_ready);
            munmap(pressure_data, sizeof(CetiPressureSample));
            return TEST_STATE_FAILED;
        }

        pressure_pass |= (pressure_data->pressure_bar > 2.0);

        // clear dynamic portion of screen
        for (int i = 4; i < 9; i++) {
            printf("\e[%d;1H\e[0K", i);
        }

        // update screen
        int width = (tui_get_screen_width() - 18) - 1;
        if (pressure_pass) {
            printf("\e[4;1H\e[0K" GREEN(PASS));
        } else {
            printf("\e[4;1H" YELLOW("In progress..."));
        }
        printf("\e[5;1HPressure : %4f", pressure_data->pressure_bar);
        // print line at threshhold
        tui_draw_horzontal_bar((pressure_data->pressure_bar > 5.0) ? 5.0 : pressure_data->pressure_bar, 5.0, 18, 5, width);
        printf("\e[4;%dH\e[96m|%.2f\e[0m\n", 18 + (int)(pressure_target * width / 5.0), pressure_target);
        printf("\e[5;%dH\e[96m|\e[0m\n", 18 + (int)(pressure_target * width / 5.0));

        // sleep for 1 second
        for (int i = 0; (i < 10) && (read(STDIN_FILENO, &input, 1) != 1) && (input == 0); i++) {
            ;
        }
    } while (input == 0);

    sem_close(pressure_data_ready);
    munmap(pressure_data, sizeof(CetiPressureSample));

    if (input == 27) {
        return TEST_STATE_TERMINATE;
    }

    fprintf(pResultsFile, "[%s]: Pressure\n", pressure_pass ? "PASS" : "FAIL");

    return (pressure_pass ? TEST_STATE_PASSED : TEST_STATE_FAILED);
}
