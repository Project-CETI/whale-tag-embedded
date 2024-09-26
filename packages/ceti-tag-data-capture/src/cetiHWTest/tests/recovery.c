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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

int64_t get_global_time_us(void) {
  struct timeval current_timeval;
  int64_t current_time_us;

  gettimeofday(&current_timeval, NULL);
  current_time_us = (int64_t)(current_timeval.tv_sec * 1000000LL) +
                    (int64_t)(current_timeval.tv_usec);
  return current_time_us;
}

// // Test for changes in light intensity
TestState test_recovery(FILE *pResultsFile) {
    char response[256];
    FILE *cmd_pipe_fd = NULL;
    FILE *rsp_pipe_fd = NULL;
    char input = 0;
    int vhf_pass = false;
    int gps_pass = false;
    time_t last_tx_time;
    char rand_str[5] = {};
    char user_input[5] = {};
    int cursor = 0;
    
    // ensure on recovery board is powered on
    cmd_pipe_fd = fopen("/opt/ceti-tag-data-capture/ipc/cetiCommand", "w");
    if (cmd_pipe_fd == NULL) {
        fprintf(pResultsFile, "[FAIL]: Recovery: Failed to open command pipe\n");
        return TEST_STATE_FAILED;
    } 
    fprintf(cmd_pipe_fd, "recovery on\n");
    fclose(cmd_pipe_fd);
    rsp_pipe_fd = fopen("/opt/ceti-tag-data-capture/ipc/cetiResponse", "r");
    if (cmd_pipe_fd == NULL) {
        fprintf(pResultsFile, "[FAIL]: Recovery: Failed to open response pipe\n");
        return TEST_STATE_FAILED;
    }
    if(!fgets(response, 256, rsp_pipe_fd)){
        fprintf(pResultsFile, "[FAIL]: Recovery: Did not receive response\n");
        fclose(rsp_pipe_fd);
        return TEST_STATE_FAILED;
    };
    do{}while(fgets(response, 256, rsp_pipe_fd));
    fclose(rsp_pipe_fd);

    // ping recovery board
    cmd_pipe_fd = fopen("/opt/ceti-tag-data-capture/ipc/cetiCommand", "w");
    if (cmd_pipe_fd == NULL) {
        fprintf(pResultsFile, "[FAIL]: Recovery: Failed to open command pipe\n");
        return TEST_STATE_FAILED;
    } 
    fprintf(cmd_pipe_fd, "recovery ping\n");
    fclose(cmd_pipe_fd);

    rsp_pipe_fd = fopen("/opt/ceti-tag-data-capture/ipc/cetiResponse", "r");
    if (cmd_pipe_fd == NULL) {
        fprintf(pResultsFile, "[FAIL]: Recovery: Failed to open response pipe\n");
        return TEST_STATE_FAILED;
    }
    if(!fgets(response, 256, rsp_pipe_fd)){
        fprintf(pResultsFile, "[FAIL]: Recovery: Did not receive response\n");
        fclose(rsp_pipe_fd);
        return TEST_STATE_FAILED;
    };
    do {
        if (memcmp(response, "pong", 4) != 0) {
            fprintf(pResultsFile, "[FAIL]: Recovery: Failed to ping recovery board\n");
            return TEST_STATE_FAILED;
        }
    } while(fgets(response, 256, rsp_pipe_fd));
    fclose(rsp_pipe_fd);

    // VHF/APRS TEST
    // generate random 4 character string
    for (int i = 0; i < sizeof(rand_str) - 1; i++){
        char val = rand() % (10 + 26 + 26);
        if (val < 10) {
            val = '0' + val;
        } else if (val < (10 + 26)) {
            val = 'A' + (val - 10);
        } else {
            val = 'a' + (val - (10 + 26));
        }
        rand_str[i] = val;
    }
    rand_str[4] = 0; 

    //send message
    cmd_pipe_fd = fopen("/opt/ceti-tag-data-capture/ipc/cetiCommand", "w");
    if (cmd_pipe_fd == NULL) {
        fprintf(pResultsFile, "[FAIL]: Recovery: Failed to open command pipe\n");
        return TEST_STATE_FAILED;
    } 
    fprintf(cmd_pipe_fd, "recovery message \"%s\"\n", rand_str);
    fclose(cmd_pipe_fd);
    rsp_pipe_fd = fopen("/opt/ceti-tag-data-capture/ipc/cetiResponse", "r");
    if (cmd_pipe_fd == NULL) {
        fprintf(pResultsFile, "[FAIL]: Recovery: Failed to open response pipe\n");
        return TEST_STATE_FAILED;
    }
    if(!fgets(response, 256, rsp_pipe_fd)){
        fprintf(pResultsFile, "[FAIL]: Recovery: Did not receive response\n");
        fclose(rsp_pipe_fd);
        return TEST_STATE_FAILED;
    };
    do{}while(fgets(response, 256, rsp_pipe_fd));
    fclose(rsp_pipe_fd);

    last_tx_time = get_global_time_us();

    printf("Instructions:\n");
    printf("    APRS: Write 4 character comment being transmitted via APRS every minute by KC1TUJ-3.\n");
    printf("    GPS:  Take tag somewhere it can receive GPS signal.\n");

    while((input == 0) && !(vhf_pass && gps_pass)){
        time_t current_time = get_global_time_us();

        printf("\e[6;1H\e[0KAPRS: %s\n", vhf_pass ? GREEN(PASS) : YELLOW("Enter code:  "));
        printf("\e[6;24H\e[0K%s\n", user_input);

        // - transmit string via APRS
        if( !vhf_pass
            && ((current_time - last_tx_time) > (60 * 1000000))
        ){
            //send message
            cmd_pipe_fd = fopen("/opt/ceti-tag-data-capture/ipc/cetiCommand", "w");
            if (cmd_pipe_fd == NULL) {
                fprintf(pResultsFile, "[FAIL]: Recovery: Failed to open command pipe\n");
                return TEST_STATE_FAILED;
            } 
            fprintf(cmd_pipe_fd, "recovery message \"%s\"\n", rand_str);
            fclose(cmd_pipe_fd);
            rsp_pipe_fd = fopen("/opt/ceti-tag-data-capture/ipc/cetiResponse", "r");
            if (cmd_pipe_fd == NULL) {
                fprintf(pResultsFile, "[FAIL]: Recovery: Failed to open response pipe\n");
                return TEST_STATE_FAILED;
            }
            if(!fgets(response, 256, rsp_pipe_fd)){
                fprintf(pResultsFile, "[FAIL]: Recovery: Did not receive response\n");
                fclose(rsp_pipe_fd);
                return TEST_STATE_FAILED;
            };
            do{}while(fgets(response, 256, rsp_pipe_fd));
            fclose(rsp_pipe_fd);
            last_tx_time = current_time;
        }

        // ToDo: check if gps pass

        printf("\e[9;1H\e[0KGPS: %s\n", gps_pass ? GREEN(PASS) : YELLOW("Waiting for lock... "));
        // printf("\e[10;4H\e[0K%s\n", sentence);


        // get user input
        if(read(STDIN_FILENO, &input, 1) == 1){
            if(!vhf_pass){
                if( (('0' <= input) && (input <= '9')) 
                    || (('A' <= input) && (input <= 'Z')) 
                    || (('a' <= input) && (input <= 'z')) 
                ){ //character
                    user_input[cursor] = input;
                    if (cursor < 3) {
                        cursor += 1;
                    } 
                    input = 0;
                    if(memcmp(rand_str, user_input, 5) == 0){
                        vhf_pass = true;
                    }
                } else if ( input == '\177') { //backspace
                    if(cursor != 0) {
                        cursor -= 1;
                    }
                    user_input[cursor] = 0;
                    input = 0;
                }
            }
        }
    }
//     //record results
    fprintf(pResultsFile, "[%s]: aprs\n", vhf_pass ? "PASS" : "FAIL");
    fprintf(pResultsFile, "[%s]: gps\n", gps_pass ? "PASS" : "FAIL");
//     recovery_kill();

    return (input == 27) ? TEST_STATE_TERMINATE
         : (vhf_pass && gps_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
}