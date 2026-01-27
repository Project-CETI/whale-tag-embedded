//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "../tests.h"
#include "../tui.h"
#include "../../cetiTagApp/cetiTag.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define RECOVERY_CODE_LENGTH 4

// External function to send CETI commands
extern int send_ceti_command(const char *command);

// Generate random 4-character alphanumeric string
static void generate_random_string(char *str) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int charset_size = sizeof(charset) - 1;
    
    for (int i = 0; i < RECOVERY_CODE_LENGTH; i++) {
        int index = rand() % charset_size;
        str[i] = charset[index];
    }
    str[RECOVERY_CODE_LENGTH] = '\0';
}

// Test APRS recovery transmission
TestState test_recovery(FILE *pResultsFile) {
    char rand_str[RECOVERY_CODE_LENGTH + 1];
    char user_input[RECOVERY_CODE_LENGTH + 1];
    char aprs_command[256];
    int cursor = 0;
    char input = '\0';
    int aprs_pass = 0;
    time_t last_tx_time = 0;
    time_t current_time = 0;
    
    // Initialize random seed
    srand(time(NULL));
    memset(user_input, 0, sizeof(user_input));
    generate_random_string(rand_str);
    
    printf("Instructions: Listen on APRS frequency for message from tag.\n");
    printf("              Enter the 4-character code received via radio.\n\n");
    
    // Send APRS message with random code
        snprintf(aprs_command, sizeof(aprs_command), "sendCommand recovery message \"%s\"", rand_str);    if (send_ceti_command(aprs_command) != 0) {
        fprintf(pResultsFile, "[FAIL]: Recovery: Failed to send APRS command\n");
        printf(RED(FAIL) " Failed to send APRS command\n");
        while ((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)) {
            ;
        }
        return TEST_STATE_FAILED;
    }  
    last_tx_time = time(NULL);
    printf("APRS message sent with code: (hidden - check your radio)\n");
    
    do {
        current_time = time(NULL); 
        // Clear dynamic portion of screen
        for (int i = 7; i < 11; i++) {
            printf("\e[%d;1H\e[0K", i);
        }
        // Display status
        if (aprs_pass) {
            printf("\e[7;1H" GREEN(PASS) " Code verified!\n");
        } else {
            printf("\e[7;1H" YELLOW("In progress...") "\n");
        }
        printf("\e[8;1HEnter code: %s", user_input);

        // Retransmit every 60 seconds if not passed
        if (!aprs_pass && ((current_time - last_tx_time) >= 60)) {
            if (send_ceti_command(aprs_command) == 0) {
                last_tx_time = current_time;
                printf("\e[10;1H(Message retransmitted)");
            }
        }
        
        // user input
        if (read(STDIN_FILENO, &input, 1) == 1) {
            if (!aprs_pass) {
                if ((('0' <= input) && (input <= '9')) ||
                    (('A' <= input) && (input <= 'Z')) ||
                    (('a' <= input) && (input <= 'z'))) {
                    // Valid alphanumeric character
                    if (cursor < RECOVERY_CODE_LENGTH) {
                        user_input[cursor] = input;
                        cursor++;
                        // Check for match
                        if (cursor == RECOVERY_CODE_LENGTH) {
                            if (memcmp(rand_str, user_input, RECOVERY_CODE_LENGTH) == 0) {
                                aprs_pass = 1;
                            } else {
                                // allow retry
                                printf("\e[9;1H" RED("Incorrect code - try again"));
                                memset(user_input, 0, sizeof(user_input));
                                cursor = 0;
                            }
                        }
                    }
                    input = '\0';
                } else if (input == '\177' || input == '\b') {
                    if (cursor > 0) {
                        cursor--;
                        user_input[cursor] = '\0';
                    }
                    input = '\0';
                } else if (input == 27) {
                    break;
                }
            } else if (input == 27) {
                break;
            } else {
                input = '\0';
            }
        }
        
    } while (!aprs_pass && input != 27);
    fprintf(pResultsFile, "[%s]: APRS Recovery Transmission\n", aprs_pass ? "PASS" : "FAIL");
    if (input == 27) {
        return TEST_STATE_TERMINATE;
    }
    if (!aprs_pass) {
        return TEST_STATE_FAILED;
    }
    return TEST_STATE_PASSED;
}