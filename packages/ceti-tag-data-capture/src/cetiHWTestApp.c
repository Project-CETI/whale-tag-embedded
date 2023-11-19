#include <sys/ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define TEST_RESULT_FILE "./test_result.txt"
#define BOLD(str)      "\e[1m" str "\e[0m"
#define UNDERLINE(str) "\e[4m" str "\e[0m"
#define RED(str)       "\e[31m" str "\e[0m"
#define GREEN(str)     "\e[32m" str "\e[0m"
#define CLEAR_SCREEN   "\e[1;1H\e[2J"
#define PASS           "PASS"
#define FAIL           "!!! FAIL !!!"


typedef enum {
    TEST_STATE_RUNNING,
    TEST_STATE_PASSED,
    TEST_STATE_FAILED,
    TEST_STATE_SKIPPED,
} TestState;

typedef TestState (TestUpdateMethod)(void);

typedef struct hardware_test_t{
    char *name;
    TestUpdateMethod *update;
    //test update
    //test monitor
    //test log
} HardwareTest;

TestUpdateMethod test_i2cverify_update;
TestUpdateMethod test_always_pass;

FILE *results_file;

HardwareTest g_test_list[] = {
    {  
        .name = "I2C Devices",
        .update = test_i2cverify_update,
    },
    {
        .name = "End",
        .update = test_always_pass,
    },
};

#define TEST_COUNT (sizeof(g_test_list)/sizeof(g_test_list[0]))

TestState test_i2cverify_update(void){
    printf("1: %02xh: Light Sensor: %s\n", 0x29, RED(FAIL));
    printf("1: %02xh:  IO Expander: %s\n", 0x38, RED(FAIL));
    printf("1: %02xh:        Depth: %s\n", 0x40, RED(FAIL));
    printf("1: %02xh:  Temperature: %s\n", 0x4C, RED(FAIL));
    printf("1: %02xh:          BMS: %s\n", 0x59, RED(FAIL));
    printf("1: %02xh:          RTC: %s\n", 0x68, RED(FAIL));
    //bus 0
        // bus 0
        // ecg
    // bus 1
        // light sensor
        // iox
        // depth
        // temperature
        // BMS
        // RTC
    return TEST_STATE_PASSED;

}

TestState test_always_pass(void){
    return TEST_STATE_PASSED;
}

int main(void) {
    int cols = 80;
    int lines = 24;

#ifdef TIOCGSIZE
    struct ttysize ts;
    ioctl(STDIN_FILENO, TIOCGSIZE, &ts);
    cols = ts.ts_cols;
    lines = ts.ts_lines;
#elif defined(TIOCGWINSZ)
    struct winsize ts;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
    cols = ts.ws_col;
    lines = ts.ws_row;
#endif /* TIOCGSIZE */


    results_file = fopen(TEST_RESULT_FILE, "wt");
    if (results_file == NULL){
        return -1;
    }

    char buffer[1024];
    time_t now = time(NULL);
    struct tm *utc_time =  gmtime(&now);
    strftime(buffer, sizeof(buffer)-1, "%b %02d %Y %02H:%02M %Z", utc_time);
    printf(CLEAR_SCREEN); //clear Screen
    fprintf(results_file, "CETI Tag Hardware Test\n");
    printf(BOLD(UNDERLINE("CETI Tag Hardware Test")) "\n");
    
    //Record tag device ID
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    printf("Tag: %s\n", hostname);
    struct hostent* h;

    //record test date
    fprintf(results_file, "Date: %s\n", buffer); //date
    printf("Date: %s\n", buffer); //date
    printf("\e[%d;1HPress any key to continue (esq - quit)",lines);
    char input = getchar();
    if(input == 27)
        return 0;
    printf(CLEAR_SCREEN); //clear Screen


    //Cycle through tests
    int passed_tests = 0;
    for(int i = 0; i < TEST_COUNT; i++){
        HardwareTest *i_test = &g_test_list[i]; //current test
        TestState i_result;

        printf(CLEAR_SCREEN);
        printf("\e[1;1H");
        fprintf(results_file, "/********************************************************\n");
        fprintf(results_file, " CETI Tag Hardware Test: (%d/%d) - %s\n", (i + 1), TEST_COUNT, i_test->name);
        fprintf(results_file, " ********************************************************/\n");

        printf(BOLD(UNDERLINE("CETI Tag Hardware Test") ": (%d/%d) - %s") "\n", (i + 1), TEST_COUNT, i_test->name);
        printf("\e[%d;1H(enter - continue; esc - quit)\e[2;1H",lines);

        while((i_result = i_test->update()) == TEST_STATE_RUNNING){
            ;
        }
        if(i_result == TEST_STATE_PASSED)
            passed_tests++;

        //get user input
        int input = getchar();
        printf(CLEAR_SCREEN); //clear Screen

        if(input == 27){
            fprintf(results_file, "HARDWARE TEST ABORTED\n");
            fclose(results_file);
            return 0;
        }
    }
    
    fprintf(results_file, "/********************************************************/\n");
    fprintf(results_file, " RESULTS: (%d/%d) PASSED", passed_tests, TEST_COUNT);
    // detect i2c sensors
        // bus 0
            // ecg
        // bus 1
            // light sensor
            // iox
            // depth
            // temperature
            // BMS
            // RTC

    // Batteries
        // check levels
        // check balance
        // check temperature
        // measure current
            // not charging
        // calculate power
    
    // Temperature
        // board temp
        // battery temp
        // pressure temp
        // cpu temp
        // gpu temp
        // verify temperature sources to one another

    // Light
        // prompt user to shine light

    // Pressure
        // prompt user to apply pressure, check for > x bar
    
    // IMU
        // prompt user to
            // rotate yaw
            // rotate pitch
            // rotate roll

    // Charging
        // prompt user to attach charger
            // measure current
                // charging
            // switch to sleep mode
            // switch to wake mode


    // Audio
        // sample noise
            // prompt user to apply signal to each input
    
    // ECG
        // prompt user to touch
            // all 3
            // + and GND
            // - and GND

    // Burn wire
        // (require user measurement + pass/fail)

    //ether

    fclose(results_file);
}