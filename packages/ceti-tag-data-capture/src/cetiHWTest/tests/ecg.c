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

TestState test_ecg(FILE *pResultsFile) {
    return TEST_STATE_FAILED;
}

// #if !ECG_OLD
// bool test_ecg_terminate = 1;
// pthread_mutex_t test_ecg_swap_lock;
// int     test_ecg_write_index = 0;
// int32_t test_ecg_circular_buffer[4000];

// void *test_ecg_acq_thread(void *paramPtr){
//     int test_ecg_write_index = 0;
//     while(!test_ecg_terminate){
//         if(ecg_adc_read_data_ready() != 0){
//             // data not ready
//             usleep(100);
//             continue;
//         }

//         test_ecg_circular_buffer[test_ecg_write_index] = ecg_adc_raw_read_data();
//         test_ecg_write_index = (test_ecg_write_index + 1) % (sizeof(test_ecg_circular_buffer)/sizeof(*test_ecg_circular_buffer));
//     }
//     return NULL;
// }

// TestState test_ecg(void){
//     char input = 0;
//     bool none_pass = false;
//     bool p_pass = false;
//     bool n_pass = false;
//     bool all_pass = false;
//     pthread_t data_acq_thread;
//     // instructions:
//     printf("Instructions: Touch the ECG leads in the following combinations");
    
//     if(init_ecg_electronics() != 0){
//         return TEST_STATE_FAILED;
//     }

//     // setup data collection thread
//     pthread_mutex_init(&test_ecg_swap_lock, NULL);
//     test_ecg_terminate = 0;
//     pthread_create(&data_acq_thread, NULL, &test_ecg_acq_thread, NULL);

//     int previous_lead_state = 0;
//     int previous_state_count = 0;
//     do {
//         //update continuity test
//         int lead_state = ecg_gpio_expander_read();

//         if(lead_state == previous_lead_state) {
//             previous_state_count++;
//             if(previous_state_count == 10){
//                 switch(lead_state & 0b11){
//                     case 0b00: all_pass = 1; break;
//                     case 0b01: p_pass = 1; break;
//                     case 0b10: n_pass = 1; break;
//                     case 0b11: none_pass = 1; break;
//                 }
//             }
//         } else {
//             previous_lead_state = lead_state;
//             previous_state_count = 0;
//         }

//         // display progress
//         printf("\e[5;1H\e[0KState: %s | %s", (lead_state & 0b10) ? " " : "+", (lead_state & 0b01) ? " " : "-");
//         printf("\e[6;1H\e[0K     None: %s\n", none_pass ? GREEN("PASS") : YELLOW("Pending..."));
//         printf("\e[7;1H\e[0K+,    GND: %s\n", p_pass ? GREEN("PASS") : YELLOW("Pending..."));
//         printf("\e[8;1H\e[0K   -, GND: %s\n", n_pass ? GREEN("PASS") : YELLOW("Pending..."));
//         printf("\e[9;1H\e[0K+, -, GND: %s\n", all_pass ? GREEN("PASS") : YELLOW("Pending..."));


        
//         // ToDo: render data in real time
//             //average or downsample to fit screen width
//         int height = tui_get_screen_height() - 13;

//         //clear old data
//         for(int i = 0; i < height; i++){
//             printf("\e[%d;1H\e[0K", i + 12);
//         }

//         //graph data
//         // if((lead_state & 0b11) == 0b00){
//         //     int end_index = test_ecg_write_index;
//         //     int start_index = (end_index - 2000 + 4000) % 4000;

//         //     //determine scaling
//         //     int32_t ecg_max = 0;
//         //     for(int i = start_index; i != end_index; i = (i + 1) % 4000){
//         //         int32_t sample_amp = abs(test_ecg_circular_buffer[i]);
//         //         if(ecg_max < sample_amp) {
//         //             ecg_max = sample_amp;
//         //         }
//         //     }

//         //     int column = 0; 
//         //     for(int i = 0; i < 2000; i += 2000/tui_get_screen_width()){
//         //         int32_t reading;
                
//         //         /* downsampling */
//         //         //  reading = test_ecg_circular_buffer[((test_ecg_write_buffer + 1 + i/100) % 11)][i % 100];
//         //         /* end downsampling */

//         //         /* averaging */

//         //         /* end averaging */

//         //         /* max amp in section */
//         //         int32_t section_amp = 0; 
//         //         for(int j = 0; j < 2000/tui_get_screen_width(); j++){
//         //             int sample = test_ecg_circular_buffer[(start_index + i + j) % 4000];
//         //             if(section_amp < abs(sample)){
//         //                 section_amp = abs(sample);
//         //                 reading = sample;
//         //             }
//         //         }
//         //         /* end max amp*/

//         //         // printf("\e[%d;%dH-", column, 12 + (height/2) - reading*height/ecg_max);
//         //         printf("\e[%d;%dH-", 12 + (height/2) - reading*height/(2*ecg_max), column);
//         //         column++;
//         //     }
//         //     pthread_mutex_unlock(&test_ecg_swap_lock);
//         // }

//         fflush(stdin);
//     } while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0));

//     //stop thread
//     test_ecg_terminate = 1;

//     // record results
//     fprintf(results_file, "[%s]: None\n", none_pass ? "PASS" : "FAIL");
//     fprintf(results_file, "[%s]: + only\n", p_pass ? "PASS" : "FAIL");
//     fprintf(results_file, "[%s]: - only\n", n_pass ? "PASS" : "FAIL");
//     fprintf(results_file, "[%s]: All\n", all_pass ? "PASS" : "FAIL");

//     // wait for user to advance screen
//     while((input == 0 && (read(STDIN_FILENO, &input, 1) != 1))){
//         ;
//     }

//     ecg_adc_cleanup();
//     ecg_gpio_expander_cleanup();

//     return (input == 27) ? TEST_STATE_TERMINATE
//          : (none_pass && p_pass && n_pass && all_pass) ? TEST_STATE_PASSED
//          : TEST_STATE_FAILED;
// }
// #endif