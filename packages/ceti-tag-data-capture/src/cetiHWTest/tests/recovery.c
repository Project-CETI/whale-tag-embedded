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

// // Test for changes in light intensity
TestState test_recovery(FILE *pResultsFile) {
    return TEST_STATE_FAILED;
}
// // 2FA for VHF, polling of GPS
// TestState test_recovery(void){
//     char input = 0;
//     int vhf_pass = false;
//     int gps_pass = false;
//     time_t last_tx_time;
//     char rand_str[5] = {};
//     char user_input[5] = {};
//     int cursor = 0;
//     char sentence[GPS_LOCATION_LENGTH];
//     APRSCallsign src_callsign = {
//         .callsign = "KC1TUJ",
//         .ssid = 3,
//     };

//     sentence[0] = 0;
//     if (recovery_init()  != 0) {
//         printf(RED(FAIL) "UART Communication Failure.\n");
//         while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)){ ; }
//         fprintf(results_file, "[FAIL]: UART communication failure.\n");
//         recovery_kill();
//         return TEST_STATE_FAILED; //imu communication error
//     }

//     // UART Communication test:
//     // - query and record recovery board's UID - requires 2 way communmication
//     // Set callsign:
//     APRSCallsign callsign;
//     recovery_set_aprs_callsign(&src_callsign);
//     if ( recovery_get_aprs_callsign(&callsign) != 0 ) {
//         printf(RED(FAIL) "Recovery query failure.\n");
//         while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)){ ; }
//         fprintf(results_file, "[FAIL]: Recovery query failure.\n");
//         recovery_kill();
//         return TEST_STATE_FAILED; //imu communication error
//     }
//     if ( memcmp(&callsign, &src_callsign, sizeof(APRSCallsign)) != 0) {
//         printf(RED(FAIL) "Recovery callsign failed to set properly.\n");
//         while((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)){ ; }
//         fprintf(results_file, "[FAIL]: Recovery callsign failed to set properly.\n");
//         recovery_kill();
//         return TEST_STATE_FAILED; //imu communication error
//     }
//     recovery_set_aprs_message_recipient(&(APRSCallsign){.callsign = "KC1QXQ", .ssid = 8});

//     // recovery_set_critical_voltage(6.2f);

//     // recovery_set_aprs_freq_mhz(145.05f);
//     // float result_freq;
//     // if(recovery_get_aprs_freq_mhz(&result_freq) != 0){
//     //     printf(RED(FAIL) "Recovery frequency query failure.\n");
//     //     while(input == 0){ read(STDIN_FILENO, &input, 1); }
//     //     fprintf(results_file, "[FAIL]: Recovery frequency query failure.\n");
//     //     recovery_kill();
//     //     return TEST_STATE_FAILED; //imu communication error
//     // }
//     // if (result_freq != 145.05f) {
//     //     printf(RED(FAIL) "Recovery frequency failed to set properly.\n");
//     //     while(input == 0){ read(STDIN_FILENO, &input, 1); }
//     //     fprintf(results_file, "[FAIL]: Recovery frequency config failure.\n");
//     //     recovery_kill();
//     //     return TEST_STATE_FAILED; //imu communication error
//     // }

//     // VHF/APRS TEST
//     // generate random 4 character string
//     for (int i = 0; i < sizeof(rand_str) - 1; i++){
//         char val = rand() % (10 + 26 + 26);
//         if (val < 10) {
//             val = '0' + val;
//         } else if (val < (10 + 26)) {
//             val = 'A' + (val - 10);
//         } else {
//             val = 'a' + (val - (10 + 26));
//         }
//         rand_str[i] = val;
//     }
//     rand_str[4] = 0;
//     recovery_message(rand_str);
//     last_tx_time = get_global_time_us();

//     printf("Instructions:\n");
//     printf("    APRS: Write 4 character comment being transmitted via APRS every minute by KC1TUJ-3.\n");
//     printf("    GPS:  Take tag somewhere it can receive GPS signal.\n");

//     while((input == 0) && !(vhf_pass && gps_pass)){
//         time_t current_time = get_global_time_us();

//         printf("\e[6;1H\e[0KAPRS: %s\n", vhf_pass ? GREEN(PASS) : YELLOW("Enter code:  "));
//         printf("\e[6;24H\e[0K%s\n", user_input);

//         // - transmit string via APRS
//         if( !vhf_pass
//             && ((current_time - last_tx_time) > (60 * 1000000))
//         ){
//             recovery_message(rand_str);
//             last_tx_time = current_time;
//         }

//         //check if gps pass
//         //offload to subprocess?
//         int result = recovery_get_gps_data(sentence, 10000);
//         if (result == 0) {
//             gps_pass = true;
//         }
//         printf("\e[9;1H\e[0KGPS: %s\n", gps_pass ? GREEN(PASS) : YELLOW("Waiting for lock... "));
//         printf("\e[10;4H\e[0K%s\n", sentence);

//         // get user input
//         if(read(STDIN_FILENO, &input, 1) == 1){
//             if(!vhf_pass){
//                 if( (('0' <= input) && (input <= '9'))
//                     || (('A' <= input) && (input <= 'Z'))
//                     || (('a' <= input) && (input <= 'z'))
//                 ){ //character
//                     user_input[cursor] = input;
//                     if (cursor < 3) {
//                         cursor += 1;
//                     }
//                     input = 0;
//                     if(memcmp(rand_str, user_input, 5) == 0){
//                         vhf_pass = true;
//                     }
//                 } else if ( input == '\177') { //backspace
//                     if(cursor != 0) {
//                         cursor -= 1;
//                     }
//                     user_input[cursor] = 0;
//                     input = 0;
//                 }
//             }
//         }
//     }
//     //record results
//     fprintf(results_file, "[%s]: aprs\n", vhf_pass ? "PASS" : "FAIL");
//     fprintf(results_file, "[%s]: gps\n", gps_pass ? "PASS" : "FAIL");
//     recovery_kill();
//     return (input == 27) ? TEST_STATE_TERMINATE
//          : (vhf_pass && gps_pass) ? TEST_STATE_PASSED
//          : TEST_STATE_FAILED;
// }