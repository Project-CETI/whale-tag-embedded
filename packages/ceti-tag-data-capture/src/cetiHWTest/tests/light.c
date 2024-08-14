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

// // Test for changes in light intensity
TestState test_light(FILE *pResultsFile) {
    const int vis_target = 500;
    const int ir_target = 1000;
    int vis_pass = 0;
    int ir_pass = 0;
    char input = '\0';

    CetiLightSample *shm_light;
    sem_t *sem_light_ready;

    // open Light shared memory object
    // open/create ipc file
    int shm_fd = shm_open(LIGHT_SHM_NAME, O_RDWR, 0644);
    if (shm_fd < 0) {
        perror("shm_open");
        fprintf(pResultsFile, "[FAIL]: Light: Failed to open shared memory\n");
        return TEST_STATE_FAILED;
    }

    // size to sample size
    if (ftruncate(shm_fd, sizeof(CetiLightSample))){
        perror("shm_fd");
        fprintf(pResultsFile, "[FAIL]: Light: Failed to size shared memory\n");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }

    // memory map address
    shm_light = mmap(NULL, sizeof(CetiLightSample), PROT_READ, MAP_SHARED, shm_fd, 0);
    if(shm_light == MAP_FAILED){
        perror("mmap");
        fprintf(pResultsFile, "[FAIL]: Light: Failed to map shared memory\n");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }

    close(shm_fd);

    // open Light shared memory object
    sem_light_ready =  sem_open(LIGHT_SEM_NAME, O_RDWR, 0644, 0);
    if(sem_light_ready == SEM_FAILED){
        perror("sem_open");
        fprintf(pResultsFile, "[FAIL]: Light: Failed to open\n");
        munmap(shm_light, sizeof(CetiLightSample));
        return TEST_STATE_FAILED;
    }

    printf("Instructions: Shine a bright light on tag light sensor\n");
    do{
        sem_wait(sem_light_ready);
        if(shm_light->error != 0){
            munmap(shm_light, sizeof(CetiLightSample));
            sem_close(sem_light_ready);
            return TEST_STATE_FAILED;
        }

        vis_pass |= (shm_light->visible > vis_target);
        ir_pass |= (shm_light->infrared > ir_target);

        //clear dynamic portion of screen
        for(int i = 4; i < 9; i++){
            printf("\e[%d;1H\e[0K", i);
        }

        //update screen
        int width = (tui_get_screen_width() - 17) - 1 ;
        if(vis_pass){
            printf("\e[4;1H\e[0K" GREEN(PASS));
        } else {
            printf("\e[4;1H" YELLOW("In progress..."));
        }
        printf("\e[5;1HVisible : %4d", shm_light->visible);
        //print line at threshhold
        tui_draw_horzontal_bar(shm_light->visible, 2048, 17, 5, width);
        printf("\e[4;%dH\e[96m|%d\e[0m\n", 17 + (vis_target*width/2048), vis_target);
        printf("\e[5;%dH\e[96m|\e[0m\n", 17 + (vis_target*width/2048));

        if(ir_pass){
            printf("\e[7;1H\e[0K" GREEN(PASS));
        } else {
            printf("\e[7;1H" YELLOW("In progress..."));
        }
        printf("\e[8;1HIR      : %4d", shm_light->infrared);
        tui_draw_horzontal_bar(shm_light->infrared, 4096, 17, 8, width);
        printf("\e[7;%dH\e[96m|%d\e[0m", 17 + (ir_target*width/4096), ir_target);
        printf("\e[8;%dH\e[96m|\e[0m\n", 17 + (ir_target*width/4096));

        //sleep for 1 second
        for(int i = 0; (i < 10) && (read(STDIN_FILENO, &input, 1) != 1) && (input == 0); i++){
            ;
        }

    }while(input == 0);
    munmap(shm_light, sizeof(CetiLightSample));
    sem_close(sem_light_ready);


    //record results
    fprintf(pResultsFile, "[%s]: Visible Light\n", vis_pass ? "PASS" : "FAIL");
    fprintf(pResultsFile, "[%s]: IR Light\n", ir_pass ? "PASS" : "FAIL");
    if(input == 27)
        return TEST_STATE_TERMINATE;

    if(!vis_pass || !ir_pass){
        return TEST_STATE_FAILED;
    }
    return TEST_STATE_PASSED;
}