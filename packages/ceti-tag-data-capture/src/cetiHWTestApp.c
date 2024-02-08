#include <ctype.h>
#include <net/if.h>
#include <pigpio.h>
#include <pthread.h> // to set CPU affinity
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
TestUpdateMethod test_audio;
TestUpdateMethod test_batteries;
TestUpdateMethod test_i2cdetect;
TestUpdateMethod test_imu;
TestUpdateMethod test_internet;
TestUpdateMethod test_light;
TestUpdateMethod test_pressure;
TestUpdateMethod test_ecg;
TestUpdateMethod test_recovery;
TestUpdateMethod test_temperature;


HardwareTest g_test_list[] = {
    { .name = "I2C Devices",      .update = test_i2cdetect, },
    { .name = "Batteries",        .update = test_batteries, },
    { .name = "Audio",            .update = test_audio, },
    { .name = "Pressure",         .update = test_pressure, },
    { .name = "ECG Connectivity", .update = test_ecg, },
    { .name = "IMU",              .update = test_imu, },
    { .name = "Temperature",      .update = test_temperature, },
    { .name = "Light",            .update = test_light, },
    { .name = "Communication",    .update = test_internet, },
    { .name = "Recovery",         .update = test_recovery, },
};
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


bool test_audio_terminate = 1;
pthread_mutex_t test_audio_swap_lock;
uint8_t test_audio_write_buffer = 0;

// divisable by SPI_BLOCK_SIZE (256*32) = 8192,
// and divisble by sample size (3*2) = 6;
#define TEST_AUDIO_LCM_BYTES 24576
#define TEST_AUDIO_BUFFER_SIZE_BYTES 884736 //~1.5 seconds of data
#define TEST_AUDIO_BUFFER_SIZE_BLOCKS (TEST_AUDIO_BUFFER_SIZE_BYTES/SPI_BLOCK_SIZE)
#define TEST_AUDIO_BUFFER_SIZE_SAMPLES (TEST_AUDIO_BUFFER_SIZE_BYTES/ (sizeof(int16_t)*CHANNELS))


uint32_t test_audio_write_block;
uint32_t test_audio_read_sample;
union __attribute__ ((scalar_storage_order ("big-endian"))) {
    uint8_t raw[TEST_AUDIO_BUFFER_SIZE_BYTES];
    char    blocks[TEST_AUDIO_BUFFER_SIZE_BLOCKS][SPI_BLOCK_SIZE];
    int16_t samples[TEST_AUDIO_BUFFER_SIZE_SAMPLES][CHANNELS];
} test_audio_data;


void *test_audio_acq_thread(void *paramPtr){
    char first_byte;

    int spi_fd = spiOpen(SPI_CE, SPI_CLK_RATE, 1);

    if (spi_fd < 0) {
        CETI_ERR("pigpio SPI initialisation failed");
        return NULL;
    }

    /* Set GPIO modes */
    gpioSetMode(AUDIO_DATA_AVAILABLE, PI_INPUT);
    start_audio_acq();

    // Discard the very first byte in the SPI stream.
    // BUG!!!!
    spiRead(spi_fd, &first_byte, 1);
    test_audio_write_block = 0;
    while(!test_audio_terminate){
        //check if data is ready
        if (!gpioRead(AUDIO_DATA_AVAILABLE)) {
            usleep(1000);
            continue;
        }

        // Wait for SPI data to *really* be available?
        // BUG!!!!! data ready pin should not be high until data is ready
        usleep(2000); //???

        //read data
        spiRead(spi_fd, test_audio_data.blocks[test_audio_write_block], SPI_BLOCK_SIZE);
        test_audio_write_block = (test_audio_write_block + 1) % TEST_AUDIO_BUFFER_SIZE_BLOCKS;

        // check that buffer is flushed
        while(gpioRead(AUDIO_DATA_AVAILABLE));
    }
    stop_audio_acq();
    usleep(100000);
    reset_audio_fifo();
    usleep(100000);
    spiClose(spi_fd);
    return NULL;
}

TestState test_audio(void){
    char input = 0;
    pthread_t data_acq_thread;
    bool channel_pass[3] = {0,0,0};
    const double target = 0.25;
    test_audio_terminate = 0;
    if (setup_audio_96kHz() != 0) {
        return TEST_STATE_FAILED;
    }
    pthread_mutex_init(&test_audio_swap_lock, NULL);
    pthread_create(&data_acq_thread, NULL, &test_audio_acq_thread, NULL);
    test_audio_read_sample = 0;
    do {
        uint32_t end_block = test_audio_write_block;
        int16_t max_audio[3] = {0, 0, 0};

        //pull latest samples from circular buffer
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wscalar-storage-order"
        uint8_t *end_ptr = (uint8_t *)test_audio_data.blocks[end_block];
        #pragma GCC diagnostic push

        if(end_ptr < (uint8_t *)test_audio_data.samples[test_audio_read_sample]){
            //transform to end of buffer
            while(test_audio_read_sample < TEST_AUDIO_BUFFER_SIZE_SAMPLES){
                // AUDIO DATA TRANSFORMATION HERE
                for(int i = 0; i < 3; i++){
                    int16_t sample_amp = abs(test_audio_data.samples[test_audio_read_sample][i]);
                    if(sample_amp > max_audio[i]){
                        max_audio[i] = sample_amp;
                    }
                }
                test_audio_read_sample++;
            }
            //wrap around
            test_audio_read_sample = 0;
        }

        //transform to end_ptr
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wscalar-storage-order"
        while((uint8_t *) test_audio_data.samples[test_audio_read_sample] < end_ptr) {
            // AUDIO DATA TRANSFORMATION HERE
            for(int i = 0; i < 3; i++){
                int16_t sample_amp = abs(test_audio_data.samples[test_audio_read_sample][i]);
                if(sample_amp > max_audio[i]){
                    max_audio[i] = sample_amp;
                }
            }
            test_audio_read_sample++;
        }
        #pragma GCC diagnostic push

       
        //ToDo: pass condition
        for(int i = 0; i < 3; i++){
            if(((double)max_audio[i])/0x7FFF > target){
                channel_pass[i] = 1;
            }
        }

        //update screen
        int width = (cols - 13) - 1 ;
        for(int i = 0; i < 3; i++){
            int line = 5 + 3*i;

            printf("\e[%d;1HCH%d: %5.3f: ", line, 1 + i, ((double)max_audio[i])/0x7FFF);
            draw_horzontal_bar(((double)max_audio[i])/0x7FFF, 1.0, 13, line, width);
            printf("\e[%d;1H\e[0K%s", line + 1, channel_pass[i] ? GREEN("PASS") : YELLOW("Pending..."));
            printf("\e[%d;%dH\e[96m|\e[0m\n", line, 13 + (int)(width*target));
        } 
        printf("\e[4;%dH\e[96m|%4.2f\e[0m\n", 13 + (int)(width*target), target);
    } while(read(STDIN_FILENO, &input, 1), input == 0);
    test_audio_terminate = 1;

    //record results
    bool all_pass = 1;
    for(int i = 0; i < 3; i++){
        fprintf(results_file, "Ch%d: %s", i, channel_pass[i] ? "PASS" : "FAIL" );
        all_pass = all_pass && channel_pass[i];
    }

    if(input == 27)
        return TEST_STATE_TERMINATE;
    
    return all_pass ? TEST_STATE_PASSED : TEST_STATE_FAILED;
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

bool test_ecg_terminate = 1;
pthread_mutex_t test_ecg_swap_lock;
int     test_ecg_write_index = 0;
int32_t test_ecg_circular_buffer[4000];

void *test_ecg_acq_thread(void *paramPtr){
    int test_ecg_write_index = 0;
    while(!test_ecg_terminate){
        if(ecg_adc_read_data_ready() != 0){
            // data not ready
            usleep(100);
            continue;
        }

        test_ecg_circular_buffer[test_ecg_write_index] = ecg_adc_raw_read_data();
        test_ecg_write_index = (test_ecg_write_index + 1) % (sizeof(test_ecg_circular_buffer)/sizeof(*test_ecg_circular_buffer));
    }
    return NULL;
}

TestState test_ecg(void){
    char input = 0;
    bool none_pass = false;
    bool p_pass = false;
    bool n_pass = false;
    bool all_pass = false;
    pthread_t data_acq_thread;
    // instructions:
    printf("Instructions: Touch the ECG leads in the following combinations");
    
    if(init_ecg_electronics() != 0){
        return TEST_STATE_FAILED;
    }

    // setup data collection thread
    pthread_mutex_init(&test_ecg_swap_lock, NULL);
    test_ecg_terminate = 0;
    pthread_create(&data_acq_thread, NULL, &test_ecg_acq_thread, NULL);

    int previous_lead_state = 0;
    int previous_state_count = 0;
    do {
        //update continuity test
        int lead_state = ecg_gpio_expander_read();

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
        printf("\e[5;1H\e[0KState: %s | %s", (lead_state & 0b10) ? " " : "+", (lead_state & 0b01) ? " " : "-");
        printf("\e[6;1H\e[0K     None: %s\n", none_pass ? GREEN("PASS") : YELLOW("Pending..."));
        printf("\e[7;1H\e[0K+,    GND: %s\n", p_pass ? GREEN("PASS") : YELLOW("Pending..."));
        printf("\e[8;1H\e[0K   -, GND: %s\n", n_pass ? GREEN("PASS") : YELLOW("Pending..."));
        printf("\e[9;1H\e[0K+, -, GND: %s\n", all_pass ? GREEN("PASS") : YELLOW("Pending..."));


        
        // ToDo: render data in real time
            //average or downsample to fit screen width
        int height = lines - 13;

        //clear old data
        for(int i = 0; i < height; i++){
            printf("\e[%d;1H\e[0K", i + 12);
        }

        //graph data
        // if((lead_state & 0b11) == 0b00){
        //     int end_index = test_ecg_write_index;
        //     int start_index = (end_index - 2000 + 4000) % 4000;

        //     //determine scaling
        //     int32_t ecg_max = 0;
        //     for(int i = start_index; i != end_index; i = (i + 1) % 4000){
        //         int32_t sample_amp = abs(test_ecg_circular_buffer[i]);
        //         if(ecg_max < sample_amp) {
        //             ecg_max = sample_amp;
        //         }
        //     }

        //     int column = 0; 
        //     for(int i = 0; i < 2000; i += 2000/cols){
        //         int32_t reading;
                
        //         /* downsampling */
        //         //  reading = test_ecg_circular_buffer[((test_ecg_write_buffer + 1 + i/100) % 11)][i % 100];
        //         /* end downsampling */

        //         /* averaging */

        //         /* end averaging */

        //         /* max amp in section */
        //         int32_t section_amp = 0; 
        //         for(int j = 0; j < 2000/cols; j++){
        //             int sample = test_ecg_circular_buffer[(start_index + i + j) % 4000];
        //             if(section_amp < abs(sample)){
        //                 section_amp = abs(sample);
        //                 reading = sample;
        //             }
        //         }
        //         /* end max amp*/

        //         // printf("\e[%d;%dH-", column, 12 + (height/2) - reading*height/ecg_max);
        //         printf("\e[%d;%dH-", 12 + (height/2) - reading*height/(2*ecg_max), column);
        //         column++;
        //     }
        //     pthread_mutex_unlock(&test_ecg_swap_lock);
        // }

        fflush(stdin);
    } while(read(STDIN_FILENO, &input, 1), input == 0);

    //stop thread
    test_ecg_terminate = 1;

    // record results
    fprintf(results_file, "[%s]: None\n", none_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: + only\n", p_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: - only\n", n_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: All\n", all_pass ? "PASS" : "FAIL");

    // wait for user to advance screen
    while((input == 0)){
        read(STDIN_FILENO, &input, 1);
    }

    ecg_adc_cleanup();
    ecg_gpio_expander_cleanup();

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

// Test of rotation along X, Y and Z
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
        bbI2CClose(IMU_BB_I2C_SDA);
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
                }
                break;
            case 5: //90 roll
                printf("\e[6;%dH" GREEN("|"), 13 + (width/4*3));
                if(85.0 <= (euler_angles.roll*180.0/M_PI) && (euler_angles.roll*180.0/M_PI) < 95.0){
                    test_index++;
                    roll_pass = 1;
                }
                break;
            default:
                break;
        }

        fflush(stdout);
    }

    //record results
    fprintf(results_file, "[%s]: roll\n", roll_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: pitch\n", pitch_pass ? "PASS" : "FAIL");
    fprintf(results_file, "[%s]: yaw\n", yaw_pass ? "PASS" : "FAIL");

    resetIMU();
    bbI2CClose(IMU_BB_I2C_SDA);
    
    return (input == 27) ? TEST_STATE_TERMINATE
         : (roll_pass && pitch_pass && yaw_pass) ? TEST_STATE_PASSED
         : TEST_STATE_FAILED;
}

// 2FA for VHF, polling of GPS
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
        recovery_kill();
        return TEST_STATE_FAILED; //imu communication error
    }

    // UART Communication test:
    // - query and record recovery board's UID - requires 2 way communmication
    APRSCallsign callsign;
    if ( recovery_get_aprs_callsign(&callsign) != 0 ) {
        printf(RED(FAIL) "Recovery query failure.\n");
        while(input == 0){ read(STDIN_FILENO, &input, 1); }
        fprintf(results_file, "[FAIL]: Recovery query failure.\n");
        recovery_kill();
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
    recovery_kill();
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

// Tests for WiFi and Eth0 connections
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

// Test for changes in light intensity
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

// Tests for changes in pressure
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

// Tests that all temperature sensors are +/- 3 degree C of their average value
TestState test_temperature(void){
    char input = 0;
    double garbage;
    int board_temp_c, battery_temp_c;
    double temp_c[5];
    bool sensor_pass[sizeof(temp_c)/sizeof(*temp_c)];

    do {
        //read temperatures
        int result = 0;
        result |= getPressureTemperature(&garbage, &temp_c[0]);
        result |= getTemperatures(&board_temp_c, &battery_temp_c);
        temp_c[1] = (double)board_temp_c;
        temp_c[2] = (double)battery_temp_c;
        temp_c[3] = get_cpu_temperature_c();
        temp_c[4] = get_gpu_temperature_c();
        if(result != 0){
            return TEST_STATE_FAILED;
        }

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
        printf("CPU     : %4.1f : %s\n", temp_c[3], sensor_pass[3] ? GREEN(PASS) :  RED(FAIL));
        printf("GPU     : %4.1f : %s\n", temp_c[4], sensor_pass[4] ? GREEN(PASS) :  RED(FAIL));
        printf("Average : %4.1f", average_temp);        
    } while(read(STDIN_FILENO, &input, 1), input == 0);

    //print results
    fprintf(results_file, "Pressure: %s\n", sensor_pass[0] ? PASS :  FAIL);
    fprintf(results_file, "Board   : %s\n", sensor_pass[1] ? PASS :  FAIL);
    fprintf(results_file, "Battery : %s\n", sensor_pass[2] ? PASS :  FAIL);
    fprintf(results_file, "CPU     : %s\n", sensor_pass[3] ? PASS :  FAIL);
    fprintf(results_file, "GPU     : %s\n", sensor_pass[4] ? PASS :  FAIL);

    if(input == 27)
        return TEST_STATE_TERMINATE;


    bool all_passed = 1;
    for(int i = 0; i < sizeof(sensor_pass)/sizeof(*sensor_pass); i++){
        all_passed &= sensor_pass[i];
    }
    return (all_passed ? TEST_STATE_PASSED : TEST_STATE_FAILED);
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
    char input = 0;
    while(read(STDIN_FILENO, &input, 1), input = 0){
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