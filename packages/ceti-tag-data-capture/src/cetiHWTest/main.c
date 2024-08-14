#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "tests.h"
#include "tui.h"

char g_process_path[256] = "/opt/ceti-tag-data-capture/bin";

#define TEST_RESULT_FILE_BASE "test_result"



typedef struct hardware_test_t{
    char *name;
    TestUpdateMethod *update;
} HardwareTest;

HardwareTest g_test_list[] = {
    { .name = "Batteries",        .update = test_ToDo, },
    { .name = "Audio",            .update = test_audio, },
    { .name = "Pressure",         .update = test_pressure, },
    { .name = "ECG",              .update = test_ToDo, },
    { .name = "IMU",              .update = test_ToDo, },
    { .name = "Temperature",      .update = test_ToDo, },
    { .name = "Light",            .update = test_light, },
    { .name = "Communication",    .update = test_internet, },
    { .name = "Recovery",         .update = test_ToDo, },
};
#define TEST_COUNT (sizeof(g_test_list)/sizeof(g_test_list[0]))

FILE *results_file;



/************************************
 * main
 */
int main(void) {
    // Get process dir path
    int bytes = readlink("/proc/self/exe", g_process_path, sizeof(g_process_path) - 1);
    while(bytes > 0) {
        if(g_process_path[bytes - 1] == '/'){
        g_process_path[bytes] = '\0';
        break;
        }
        bytes--;
    }

    //TODO: use DBUS to ensure ceti-tag-capture-app is running

    tui_initialize();
    tui_update_screen_size();

    struct timeval te;
    char test_result_file_path[256];
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    snprintf(test_result_file_path, sizeof(test_result_file_path) - 1, "/data/" TEST_RESULT_FILE_BASE "_%lld.txt", milliseconds);
    results_file = fopen(test_result_file_path, "wt");
    if (results_file == NULL){
        return -1;
    }

    char buffer[1024];
    time_t now = time(NULL);
    srand(now); //seed test randomness
    struct tm *utc_time =  gmtime(&now);
    strftime(buffer, sizeof(buffer)-1, "%b %02d %Y %02H:%02M %Z", utc_time);
    // printf(CLEAR_SCREEN); //clear Screen
    fprintf(results_file, "CETI Tag Hardware Test\n");
    printf(BOLD(UNDERLINE("CETI Tag Hardware Test")) "\n");
    
    //Record tag device ID
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    printf("Tag: %s\n", hostname);
    fprintf(results_file, "Tag: %s\n", hostname);

    //record test date
    fprintf(results_file, "Date: %s\n", buffer); //date
    printf("Date: %s\n", buffer); //date
    tui_goto(1,tui_get_screen_height());
    printf("Press any key to continue (esq - quit)");
    char input = 0;
    while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)){
        ;
    }

    printf(CLEAR_SCREEN); //clear Screen
    if(input == 27)
        return 0;


    //Cycle through tests
    int passed_tests = 0;
    for(int i = 0; i < TEST_COUNT; i++){
        HardwareTest *i_test = &g_test_list[i]; //current test
        TestState i_result;

        //update screen size
        tui_update_screen_size();

        printf(CLEAR_SCREEN);
        tui_goto(1,1);
        fprintf(results_file, "/********************************************************\n");
        fprintf(results_file, " * CETI Tag Hardware Test: (%d/%ld) - %s\n", (i + 1), TEST_COUNT, i_test->name);
        fprintf(results_file, " */\n");

        printf(BOLD(UNDERLINE("CETI Tag Hardware Test") ": (%d/%ld) - %s") "\n", (i + 1), TEST_COUNT, i_test->name);
        tui_goto(1,tui_get_screen_height());
        printf("(enter - continue; esc - quit)");
        tui_goto(1,2);
        while((i_result = i_test->update(results_file)) == TEST_STATE_RUNNING){
            tui_update_screen_size();
        }
        if (i_result == TEST_STATE_PASSED) {
            passed_tests++;
            fprintf(results_file, "TEST PASSED\n\n");
        } else if(i_result == TEST_STATE_FAILED){
            fprintf(results_file, "TEST FAILED!!!\n\n");
        } else if(i_result == TEST_STATE_TERMINATE){
            fprintf(results_file, "HARDWARE TEST ABORTED\n");
            break;
        }
    }
    printf(CLEAR_SCREEN);
    
    fprintf(results_file, "/********************************************************/\n");
    fprintf(results_file, " RESULTS: (%d/%ld) PASSED\n", passed_tests, TEST_COUNT);
    
    fclose(results_file);
}