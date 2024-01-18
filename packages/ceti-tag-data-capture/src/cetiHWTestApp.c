#include <ctype.h>
#include <net/if.h>
#include <pigpio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "battery.h"
#include "fpga.h"
#include "sensors/light.h"

int g_exit = 0;
int g_stopAcquisition = 0;

#define TEST_RESULT_FILE "./test_result.txt"
#define BOLD(str)      "\e[1m" str "\e[0m"
#define UNDERLINE(str) "\e[4m" str "\e[0m"
#define RED(str)       "\e[31m" str "\e[0m"
#define GREEN(str)     "\e[32m" str "\e[0m"
#define YELLOW(str)    "\e[33m" str "\e[0m"
#define CYAN(str)      "\e[96m" str "\e[0m"
#define CLEAR_SCREEN   "\e[1;1H\e[2J"
#define PASS           "PASS"
#define FAIL           "!!! FAIL !!!"


typedef enum {
    TEST_STATE_RUNNING,
    TEST_STATE_PASSED,
    TEST_STATE_FAILED,
    TEST_STATE_SKIPPED,
    TEST_STATE_TERMINATE,
} TestState;

typedef TestState (TestUpdateMethod)(void);

typedef struct hardware_test_t{
    char *name;
    TestUpdateMethod *update;
    //test update
    //test monitor
    //test log
} HardwareTest;

TestUpdateMethod test_ToDo;
TestUpdateMethod test_batteries;
TestUpdateMethod test_i2cdetect;
TestUpdateMethod test_imu;
TestUpdateMethod test_internet;
TestUpdateMethod test_light;
TestUpdateMethod test_pressure;
TestUpdateMethod test_ecg;
TestUpdateMethod test_recovery;


HardwareTest g_test_list[] = {
    { .name = "I2C Devices",      .update = test_i2cdetect, },
    { .name = "Batteries",        .update = test_batteries, },
    { .name = "Audio",            .update = test_ToDo, },
    { .name = "Pressure",         .update = test_pressure, },
    { .name = "ECG Connectivity", .update = test_ToDo, },
    { .name = "IMU",              .update = test_imu, },
    { .name = "Temperature",      .update = test_ToDo, },
    { .name = "Light",            .update = test_light, },
    { .name = "Communication",    .update = test_internet, },
    { .name = "Recovery",         .update = test_recovery, },
};
    // ECG 

    // Temperature
        // board temp
        // battery temp
        // pressure temp
        // cpu temp
        // gpu temp
        // verify temperature sources to one another

    // Charging
        // prompt user to attach charger
            // measure current
                // charging
            // switch to sleep mode
            // switch to wake mode

    // Audio
        // sample noise
            // prompt user to apply signal to each input

    // Burn wire
        // (require user measurement + pass/fail)

FILE *results_file;
int cols = 80;
int lines = 24;

#define TEST_COUNT (sizeof(g_test_list)/sizeof(g_test_list[0]))

void draw_horzontal_bar(double value, double val_max, int x, int y, int w);
void draw_inv_horzontal_bar(double value, double val_min, int x, int y, int w);

/*******************************************
 * TESTS
 */
TestState test_ToDo(void){
    //display message
    printf(YELLOW("TODO: IMPLEMENT THIS TEST!!!!!!!\n"));

    //get user input
    char input = '\0';
    while(input == 0){
        read(STDIN_FILENO, &input, 1);
    }

    if(input == 27)
        return TEST_STATE_TERMINATE;

    return TEST_STATE_PASSED;
}

TestState test_batteries(void){
    const double cell_min_v = 3.1;
    const double cell_max_v = 4.2;
    const double cell_balance_limit_v = 0.05;

    double v1_v, v2_v, current;
  
    //perform test
    if(getBatteryData(&v1_v, &v2_v, &current) != 0){
        printf(RED(FAIL) " BMS Communication Failure.\n");
        return TEST_STATE_FAILED;
    }

    bool cell1_pass = ((cell_min_v < v1_v) && (v1_v < cell_max_v));
    bool cell2_pass = ((cell_min_v < v2_v) && (v2_v < cell_max_v));
    double balance = v1_v - v2_v;
    balance = (balance < 0) ? -balance : balance;
    bool balance_pass = (balance < cell_balance_limit_v); 

    //record results
    fprintf(results_file, "[%s]: Cell 1 (%4.2f V)\n", cell1_pass ? "PASS" : "FAIL", v1_v);
    fprintf(results_file, "[%s]: Cell 2 (%4.2f V)\n", cell2_pass ? "PASS" : "FAIL", v2_v);
    fprintf(results_file, "[%s]: Balance (%4.2f mV)\n", balance_pass ? "PASS" : "FAIL", balance);
    
    //display results
    printf("Cell 1 (%4.2f V): %s\n", v1_v, cell1_pass ? GREEN(PASS) : RED(FAIL));
    printf("Cell 2 (%4.2f V): %s\n", v2_v, cell2_pass ? GREEN(PASS) : RED(FAIL));
    printf("Cell diff (%3.0f mV): %s\n", 1000*balance, balance_pass ? GREEN(PASS) : RED(FAIL));

    //get user input
    char input = '\0';
    while(input == 0){
        read(STDIN_FILENO, &input, 1);
    }

    if(input == 27)
        return TEST_STATE_TERMINATE;
    
    if(cell1_pass && cell2_pass && balance_pass){
        return TEST_STATE_PASSED;
    }

    return TEST_STATE_FAILED;
}

TestState test_ecg(void){
    char input;
    bool none_pass = false;
    bool p_pass = false;
    bool n_pass = false;
    bool all_pass = false;
    int state_count = 0;

    // instructions:
    printf("Instructions: Touch the ECG leads in the following combinations");
    
    // ToDo: initialize ecg

    while((input == 0) && !(none_pass && p_pass && n_pass && all_pass)){
        // ToDo implement reading eletrode state
        // waiting on ECG update approval


        // display progress
        printf("\e[4;1H\e[0K     None: %s\n", none_pass ? GREEN("PASS") : YELLOW("Pending..."));
        printf("\e[4;1H\e[0K+,    GND: %s\n", p_pass ? GREEN("PASS") : YELLOW("Pending..."));
        printf("\e[4;1H\e[0K   -, GND: %s\n", n_pass ? GREEN("PASS") : YELLOW("Pending..."));
        printf("\e[4;1H\e[0K+, -, GND: %s\n", all_pass ? GREEN("PASS") : YELLOW("Pending..."));
        fflush(stdin);
        read(STDIN_FILENO, &input, 1); 
    }

    // record results
    fprintf(results_file, "[%s]: None\n", none_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: + only\n", p_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: - only\n", n_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: All\n", all_pass ? "PASS" : "FAIL");

    // wait for user to advance screen
    while((input == 0)){
        read(STDIN_FILENO, &input, 1);
    }

    return (input == 27) ? TEST_STATE_TERMINATE
         : (none_pass && p_pass && n_pass && all_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
}

TestState test_i2cdetect(void){
    int fd;
    // light sensor
    int ecg_adc_valid = 0;
    int ecg_iox_valid = 0;
    int light_valid = 0;
    int bms_valid = 0;
    int temp_valid = 0;
    int iox_valid = 0;
    int depth_valid = 0;
    int rtc_valid = 0;

    //perform tests
    if((ecg_adc_valid = !((fd = i2cOpen(0, ECG_ADC_I2C_ADDRESS, 0)) < 0)))
        i2cClose(fd);

    if((ecg_iox_valid = !((fd = i2cOpen(0, ECG_GPIO_EXPANDER_I2C_ADDRESS, 0)) < 0)))
        i2cClose(fd);

    light_valid = (light_verify() == 0);

    if((bms_valid = !((fd = i2cOpen(1, ADDR_BATT_GAUGE, 0)) < 0)))
        i2cClose(fd);

    if((temp_valid = !((fd = i2cOpen(1, ADDR_BOARDTEMPERATURE, 0)) < 0)))
        i2cClose(fd);

    if((iox_valid = !((fd = i2cOpen(1, ADDR_MAINTAG_IOX, 0)) < 0)))
        i2cClose(fd);

    if((depth_valid = !((fd = i2cOpen(1, ADDR_PRESSURETEMPERATURE, 0)) < 0)))
        i2cClose(fd);

    if((rtc_valid = !((fd = i2cOpen(1, ADDR_RTC, 0)) < 0)))
        i2cClose(fd);

    // record results
    fprintf(results_file, "[%s]: 0: 0x%02x: ECG IOX\n", ecg_iox_valid ? "PASS" : "FAIL", ECG_GPIO_EXPANDER_I2C_ADDRESS);
    fprintf(results_file, "[%s]: 0: 0x%02x: ECG ADC\n", ecg_adc_valid ? "PASS" : "FAIL", ECG_ADC_I2C_ADDRESS);

    fprintf(results_file, "[%s]: 1: 0x%02x: Light Sensor\n", light_valid ? "PASS" : "FAIL", ADDR_LIGHT);
    fprintf(results_file, "[%s]: 1: 0x%02x: Burnwire IOX\n", iox_valid ? "PASS" : "FAIL", ADDR_MAINTAG_IOX);
    fprintf(results_file, "[%s]: 1: 0x%02x: Depth\n", depth_valid ? "PASS" : "FAIL", ADDR_PRESSURETEMPERATURE);
    fprintf(results_file, "[%s]: 1: 0x%02x: Temperature\n", temp_valid ? "PASS" : "FAIL", ADDR_BOARDTEMPERATURE);
    fprintf(results_file, "[%s]: 1: 0x%02x: BMS\n", bms_valid ? "PASS" : "FAIL", ADDR_BATT_GAUGE);
    fprintf(results_file, "[%s]: 1: 0x%02x: RTC\n", rtc_valid ? "PASS" : "FAIL", ADDR_RTC);

    // display results
    printf("0: 0x%02x:      ECG IOX: %s\n", ECG_GPIO_EXPANDER_I2C_ADDRESS, ecg_iox_valid ? GREEN(PASS) :  RED(FAIL));
    printf("0: 0x%02x:      ECG ADC: %s\n", ECG_ADC_I2C_ADDRESS, ecg_adc_valid ? GREEN(PASS) :  RED(FAIL));
    printf("1: 0x%02x: Light Sensor: %s\n", ADDR_LIGHT, light_valid ? GREEN(PASS) :  RED(FAIL));
    printf("1: 0x%02x: Burnwire IOX: %s\n", ADDR_MAINTAG_IOX, iox_valid ? GREEN(PASS) :  RED(FAIL));
    printf("1: 0x%02x:        Depth: %s\n", ADDR_PRESSURETEMPERATURE, depth_valid ? GREEN(PASS) :  RED(FAIL));
    printf("1: 0x%02x:  Temperature: %s\n", ADDR_BOARDTEMPERATURE, temp_valid ? GREEN(PASS) :  RED(FAIL));
    printf("1: 0x%02x:          BMS: %s\n", ADDR_BATT_GAUGE, bms_valid ? GREEN(PASS) :  RED(FAIL));
    printf("1: 0x%02x:          RTC: %s\n", ADDR_RTC, rtc_valid ? GREEN(PASS) :  RED(FAIL));
    fflush(stdin);

    //get user input
    char input = '\0';
    while(input == 0){
        read(STDIN_FILENO, &input, 1);
    }

    if(input == 27)
        return TEST_STATE_TERMINATE;

    if(light_valid && bms_valid && temp_valid && iox_valid && depth_valid && 
       rtc_valid 
    ) {
        return TEST_STATE_PASSED;
    }
    return TEST_STATE_FAILED;
}

TestState test_imu(void){
    char input = '\0';
    int roll_pass = 0;
    int pitch_pass = 0;
    int yaw_pass = 0;
    int test_index = 0;

    //initialize IMU
    if(setupIMU(IMU_QUAT_ENABLED) < 0) {
        printf(RED(FAIL) " IMU Communication Failure.\n");
        while(input == 0){ read(STDIN_FILENO, &input, 1); }
        return TEST_STATE_FAILED; //imu communication error
    }

    //get start angle
    printf("Instructions: rotate the tag to meet each green target position below\n\n");

    while(read(STDIN_FILENO, &input, 1), input == 0){
        EulerAngles_f64 euler_angles = {};
        
        //update test
        int report_id_updated = imu_get_euler_angles(&euler_angles);
        if(report_id_updated == -1) { // no data received or wrong data type
            continue; // Note that we are streaming 4 reports at 50 Hz, so we expect data to be available at about 200 Hz
        }

        euler_angles.yaw *= -1.0;

        //update live view
        int width = (cols - 13) - 2 ;
        printf("\e[3;11H-180\e[3;%dH0\e[3;%dH180\n", 13 + width/2, cols - 2 - 1);
        printf("\e[4;1H\e[0KPitch: %4s\e[4;13H|\e[4;%dH|\e[4;%dH|\n", pitch_pass ? GREEN(PASS) : "",13 + width/2, cols - 2);
        printf("\e[5;1H\e[0KYaw  : %4s\e[5;13H|\e[5;%dH|\e[5;%dH|\n", yaw_pass ? GREEN(PASS) : "", 13 + width/2, cols - 2);
        printf("\e[6;1H\e[0KRoll : %4s\e[6;13H|\e[6;%dH|\e[6;%dH|\n", roll_pass ? GREEN(PASS) : "", 13 + width/2, cols - 2);

        //draw reading position
        if(euler_angles.pitch > 0){
            draw_horzontal_bar(euler_angles.pitch, M_PI, 13 + width/2, 4, width/2);
        }
        else if (euler_angles.pitch < 0) {
            draw_inv_horzontal_bar(euler_angles.pitch, -M_PI, 13, 4, width/2);
        }

        if(euler_angles.yaw > 0){
            draw_horzontal_bar(euler_angles.yaw, M_PI, 13 + width/2, 5, width/2);
        }
        else if (euler_angles.yaw < 0) {
            draw_inv_horzontal_bar(euler_angles.yaw, -M_PI, 13, 5, width/2);
        }

        if(euler_angles.roll > 0){
            draw_horzontal_bar(euler_angles.roll, M_PI, 13 + width/2, 6, width/2);
        }
        else if (euler_angles.roll < 0) {
            draw_inv_horzontal_bar(euler_angles.roll, -M_PI, 13, 6, width/2);
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
                    roll_pass = 1;
                }
                break;
            case 5: //90 roll
                printf("\e[6;%dH" GREEN("|"), 13 + (width/4*3));
                if(85.0 <= (euler_angles.roll*180.0/M_PI) && (euler_angles.roll*180.0/M_PI) < 95.0){
                    test_index++;
                }
                break;
        }



        fflush(stdout);
    }

    //record results
    fprintf(results_file, "[%s]: roll\n", roll_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: pitch\n", pitch_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: yaw\n", yaw_pass ? "PASS" : "FAIL");

    resetIMU();
    
    return (input == 27) ? TEST_STATE_TERMINATE
         : (roll_pass && pitch_pass && yaw_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
}

TestState test_recovery(void){
    char input;
    int vhf_pass = false;
    int gps_pass = false;
    time_t last_tx_time;
    char rand_str[5] = {};
    char user_input[5] = {};
    int cursor = 0;
    char sentence[GPS_LOCATION_LENGTH];
    APRSCallsign src_callsign = {
        .callsign = "KC1TUJ",
        .ssid = 3,
    };
    
    sentence[0] = 0;

    if (recovery_init()  != 0) {
        printf(RED(FAIL) "UART Communication Failure.\n");
        while(input == 0){ read(STDIN_FILENO, &input, 1); }
        fprintf(results_file, "[FAIL]: UART communication failure.\n");
        return TEST_STATE_FAILED; //imu communication error
    }

    // UART Communication test:
    // - query and record recovery board's UID - requires 2 way communmication
    APRSCallsign callsign;
    if ( recovery_get_aprs_callsign(&callsign) != 0 ) {
        printf(RED(FAIL) "Recovery query failure.\n");
        while(input == 0){ read(STDIN_FILENO, &input, 1); }
        fprintf(results_file, "[FAIL]: Recovery query failure.\n");
        return TEST_STATE_FAILED; //imu communication error
    }

    // Set callsign:
    recovery_set_aprs_callsign(&src_callsign);
    recovery_set_aprs_message_recipient(&(APRSCallsign){.callsign = "KC1QXQ", .ssid = 8});
    recovery_on();

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
    recovery_message(rand_str);
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
            recovery_message(rand_str);
            last_tx_time = current_time;
        }

        //check if gps pass
        //offload to subprocess?
        int result = recovery_get_gps_data(sentence, 10000);
        if (result == 0) {
            gps_pass = true;
        }
        printf("\e[9;1H\e[0KGPS: %s\n", gps_pass ? GREEN(PASS) : YELLOW("Waiting for lock... "));
        printf("\e[10;4H\e[0K%s\n", sentence);


        // get user input
        read(STDIN_FILENO, &input, 1);
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
    //record results
    fprintf(results_file, "[%s]: aprs\n", vhf_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: gps\n", gps_pass ? "PASS" : "FAIL");

    return (input == 27) ? TEST_STATE_TERMINATE
         : (vhf_pass && gps_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
}

// Function to check the status of a network interface
int checkInterfaceStatus(const char *interface) {
    int sock;
    struct ifreq ifr;

    // Create a socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        // perror("socket");
        // exit(EXIT_FAILURE);
        return 0;
    }

    // Set the interface name
    strncpy(ifr.ifr_name, interface, IFNAMSIZ);

    // Get the interface flags
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        // perror("ioctl");
        close(sock);
        return 0;
        // exit(EXIT_FAILURE);
    }

    // Check if the interface is up
    close(sock);
    return (ifr.ifr_flags & IFF_UP) ? 1 : 0;
}

TestState test_internet(void){
    int wlan0_pass = 0;
    int eth0_pass = 0;
    char input = 0;

    printf("Instructions: connect ethernet and wifi\n\n");

    while((input == 0) && !(eth0_pass && wlan0_pass)){
        //test connection
        if(!wlan0_pass)
            wlan0_pass |= checkInterfaceStatus("wlan0");
        
        if (!eth0_pass)
            eth0_pass |= checkInterfaceStatus("eth0");

        //display progress
        printf("\e[4;1H\e[0Kwlan0: %s\n", wlan0_pass ? GREEN(PASS) : YELLOW("Waiting for connection..."));
        printf("\e[7;1H\e[0K eth0: %s\n", eth0_pass ? GREEN(PASS) : YELLOW("Waiting for connection..."));
        fflush(stdin);
        read(STDIN_FILENO, &input, 1);    
    }

    //record results
    fprintf(results_file, "[%s]: wlan0\n", wlan0_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: eth0\n", eth0_pass ? "PASS" : "FAIL");

    //wait for user to advance screen
    while((input == 0)){
        read(STDIN_FILENO, &input, 1);
    }

    return (input == 27) ? TEST_STATE_TERMINATE
         : (eth0_pass && wlan0_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
}

TestState test_light(void){
    const int vis_target = 500;
    const int ir_target = 1000;
    int vis_light, ir_light;
    int vis_pass = 0;
    int ir_pass = 0;
    char input = '\0';
    printf("Instructions: Shine a bright light on tag light sensor\n");

    do{
        int result = getAmbientLight(&vis_light, &ir_light);
        if(result != 0){
            return TEST_STATE_FAILED;
        }

        vis_pass |= (vis_light > vis_target);
        ir_pass |= (ir_light > ir_target);

        //clear dynamic portion of screen
        for(int i = 4; i < 9; i++){
            printf("\e[%d;1H\e[0K", i);
        }

        //update screen
        int width = (cols - 17) - 1 ;
        if(vis_pass){
            printf("\e[4;1H\e[0K" GREEN(PASS));
        } else {
            printf("\e[4;1H" YELLOW("In progress..."));
        }
        printf("\e[5;1HVisible : %4d", vis_light);
        //print line at threshhold
        draw_horzontal_bar(vis_light, 2048, 17, 5, width);
        printf("\e[4;%dH\e[96m|%d\e[0m\n", 17 + (vis_target*width/2048), vis_target);
        printf("\e[5;%dH\e[96m|\e[0m\n", 17 + (vis_target*width/2048));

        if(ir_pass){
            printf("\e[7;1H\e[0K" GREEN(PASS));
        } else {
            printf("\e[7;1H" YELLOW("In progress..."));
        }
        printf("\e[8;1HIR      : %4d", ir_light);
        draw_horzontal_bar(ir_light, 4096, 17, 8, width);
        printf("\e[7;%dH\e[96m|%d\e[0m", 17 + (ir_target*width/4096), ir_target);
        printf("\e[8;%dH\e[96m|\e[0m\n", 17 + (ir_target*width/4096));

        //sleep for 1 second
        for(int i = 0; (i < 10) && (input == 0); i++){
            read(STDIN_FILENO, &input, 1);
        }

    }while(input == 0);

    if(input == 27)
        return TEST_STATE_TERMINATE;

    //record results
    fprintf(results_file, "[%s]: Visible Light\n", vis_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: IR Light\n", ir_pass ? "PASS" : "FAIL");

    if(!vis_pass || !ir_pass){
        return TEST_STATE_FAILED;
    }
    return TEST_STATE_PASSED;
}

TestState test_pressure(void){
    const double pressure_target = 2.0;
    double pressure_bar, temperature_c;
    int pressure_pass = 0;
    char input;
    printf( "Instructions: Use a syringe to apply pressure to the tag's depth sensor\n");

    do{
        int result = getPressureTemperature(&pressure_bar, &temperature_c);
        if(result != 0){
            return TEST_STATE_FAILED;
        }

        pressure_pass |= (pressure_bar > 2.0);

        //clear dynamic portion of screen
        for(int i = 4; i < 9; i++){
            printf("\e[%d;1H\e[0K", i);
        }

        //update screen
        int width = (cols - 18) - 1 ;
        if(pressure_pass){
            printf("\e[4;1H\e[0K" GREEN(PASS));
        } else {
            printf("\e[4;1H" YELLOW("In progress..."));
        }
        printf("\e[5;1HPressure : %4f", pressure_bar);
        //print line at threshhold
        draw_horzontal_bar((pressure_bar > 5.0) ? 5.0 : pressure_bar, 5.0, 18, 5, width);
        printf("\e[4;%dH\e[96m|%.2f\e[0m\n", 18 + (int)(pressure_target*width/5.0), pressure_target);
        printf("\e[5;%dH\e[96m|\e[0m\n", 18 + (int)(pressure_target*width/5.0));

        //sleep for 1 second
        for(int i = 0; (i < 10) && (input == 0); i++){
            read(STDIN_FILENO, &input, 1);
        }
    }while(input == 0);

    if(input == 27)
        return TEST_STATE_TERMINATE;

    fprintf(results_file, "[%s]: Pressure\n", pressure_pass ? "PASS" : "FAIL");

    return (pressure_pass ? TEST_STATE_PASSED : TEST_STATE_FAILED);
}


/************************************
 * CLI control functions 
 */
struct termios og_termios;

void disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_termios);
}

void enableRawMode(void) {
    tcgetattr(STDIN_FILENO, &og_termios);
    atexit(disableRawMode);

    struct termios raw = og_termios;
    raw.c_iflag &= ~(ICRNL | IXON);
    // raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void draw_horzontal_bar(double value, double val_max, int x, int y, int w){
    printf("\e[%d;%dH", y, x);
    printf("\e[106m");
    for(int i = 0; i < w*value/val_max; i++){
        printf(" ");
    }
    printf("\e[0m\e[0K");
}

void draw_inv_horzontal_bar(double value, double val_min, int x, int y, int w){
    printf("\e[%d;%dH", y, (uint32_t)(x + w*(1.0 - (value/val_min))));
    printf("\e[106m");
    for(int i = w*(1.0 - (value/val_min)); i < w; i++){
        printf(" ");

    }
    printf("\e[0m\e[0K");
}


/************************************
 * main
 */
int main(void) {
    if (gpioInitialise() < 0) {
        CETI_ERR("Failed to initialize pigpio");
        return 1;
    }
    atexit(gpioTerminate);

    init_fpga();
    light_wake();
    enableRawMode();
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

        //update screen size
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

        printf(CLEAR_SCREEN);
        printf("\e[1;1H");
        fprintf(results_file, "/********************************************************\n");
        fprintf(results_file, " * CETI Tag Hardware Test: (%d/%ld) - %s\n", (i + 1), TEST_COUNT, i_test->name);
        fprintf(results_file, " */\n");

        printf(BOLD(UNDERLINE("CETI Tag Hardware Test") ": (%d/%ld) - %s") "\n", (i + 1), TEST_COUNT, i_test->name);
        printf("\e[%d;1H(enter - continue; esc - quit)\e[2;1H",lines);

        while((i_result = i_test->update()) == TEST_STATE_RUNNING){
            ;
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