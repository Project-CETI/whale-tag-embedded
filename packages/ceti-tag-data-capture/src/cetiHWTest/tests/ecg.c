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

TestState test_ecg(FILE *pResultsFile) {
    char input = 0;
    int32_t none_pass = 0;
    int32_t p_pass = 0;
    int32_t n_pass = 0;
    int32_t all_pass = 0;

    CetiEcgBuffer *shm_ecg;
    sem_t *sem_ecg_sample_ready;

    // instructions:
    printf("Instructions: Touch the ECG leads in the following combinations");
    

    // === open ecg shared memory ===
    int shm_fd = shm_open(ECG_SHM_NAME, O_RDWR, 0444);
    if (shm_fd < 0) {
        fprintf(pResultsFile, "[FAIL]: ECG: Failed to open shared memory\n");
        perror("shm_open");
        return TEST_STATE_FAILED;
    }
    // size to sample size
    if (ftruncate(shm_fd, sizeof(CetiEcgBuffer))){
        fprintf(pResultsFile, "[FAIL]: ECG: Failed to size shared memory\n");
        perror("ftruncate");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }
    // memory map address
    shm_ecg = mmap(NULL, sizeof(CetiEcgBuffer), PROT_READ , MAP_SHARED, shm_fd, 0);
    if(shm_ecg == MAP_FAILED){
        perror("mmap");
        fprintf(pResultsFile, "[FAIL]: ECG: Failed to map shared memory\n");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }
    close(shm_fd);

    sem_ecg_sample_ready = sem_open(ECG_SAMPLE_SEM_NAME, O_RDWR, 0444, 0);
    if(sem_ecg_sample_ready == SEM_FAILED){
        perror("sem_open");
        fprintf(pResultsFile, "[FAIL]: ECG: Failed to open ecg sample semaphore\n");
        munmap(shm_ecg, sizeof(CetiEcgBuffer));
        return TEST_STATE_FAILED;
    }



    int previous_lead_state = 0;
    int previous_state_count = 0;
    do {
        //wait for sample
        sem_wait(sem_ecg_sample_ready);

        //update continuity test
        int lead_state = ((shm_ecg->leadsOff_readings_p[shm_ecg->page][shm_ecg->sample] != 0) << 1) | ((shm_ecg->leadsOff_readings_n[shm_ecg->page][shm_ecg->sample] != 0) << 0);

        if(lead_state == previous_lead_state) {
            previous_state_count++;
            if(previous_state_count == 10){
                switch(lead_state & 0b11){
                    case 0b00: all_pass = 1; break;
                    case 0b01: p_pass = 1; break;
                    case 0b10: n_pass = 1; break;
                    case 0b11: none_pass = 1; break;
                }
            }
        } else {
            previous_lead_state = lead_state;
            previous_state_count = 0;
        }

        // display progress
        tui_goto(1, 5); printf("State: %s | %s", (lead_state & 0b10) ? " " : "+", (lead_state & 0b01) ? " " : "-");
        tui_goto(1, 6); printf("     None: %-15s", none_pass ? GREEN("PASS      ") : YELLOW("Pending..."));
        tui_goto(1, 7); printf("+,    GND: %-15s", p_pass ? GREEN("PASS      ") : YELLOW("Pending..."));
        tui_goto(1, 8); printf("   -, GND: %-15s", n_pass ? GREEN("PASS      ") : YELLOW("Pending..."));
        tui_goto(1, 9); printf("+, -, GND: %-15s", all_pass ? GREEN("PASS      ") : YELLOW("Pending..."));
        
//         // ToDo: render data in real time
//             //average or downsample to fit screen width
//         int height = tui_get_screen_height() - 13;

//         //clear old data
//         for(int i = 0; i < height; i++){
//             printf("\e[%d;1H\e[0K", i + 12);
//         }

        fflush(stdin);
    } while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0));

//     // record results
    fprintf(pResultsFile, "[%s]: None\n", none_pass ? "PASS" : "FAIL");
    fprintf(pResultsFile, "[%s]: + only\n", p_pass ? "PASS" : "FAIL");
    fprintf(pResultsFile, "[%s]: - only\n", n_pass ? "PASS" : "FAIL");
    fprintf(pResultsFile, "[%s]: All\n", all_pass ? "PASS" : "FAIL");

    sem_close(sem_ecg_sample_ready);
    munmap(shm_ecg, sizeof(CetiEcgBuffer));

    return (input == 27) ? TEST_STATE_TERMINATE
         : (none_pass && p_pass && n_pass && all_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
}
