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

TestState test_batteries(FILE *pResultsFile) {
    return TEST_STATE_FAILED;
}

// TestState test_batteries(void){
//     const double cell_min_v = 3.1;
//     const double cell_max_v = 4.2;
//     const double cell_balance_limit_v = 0.05;

//     double v1_v, v2_v, current;
  
//     //perform test
//     if(getBatteryData(&v1_v, &v2_v, &current) != 0){
//         printf(RED(FAIL) " BMS Communication Failure.\n");
//         return TEST_STATE_FAILED;
//     }

//     bool cell1_pass = ((cell_min_v < v1_v) && (v1_v < cell_max_v));
//     bool cell2_pass = ((cell_min_v < v2_v) && (v2_v < cell_max_v));
//     double balance = v1_v - v2_v;
//     balance = (balance < 0) ? -balance : balance;
//     bool balance_pass = (balance < cell_balance_limit_v); 

//     //record results
//     fprintf(results_file, "[%s]: Cell 1 (%4.2f V)\n", cell1_pass ? "PASS" : "FAIL", v1_v);
//     fprintf(results_file, "[%s]: Cell 2 (%4.2f V)\n", cell2_pass ? "PASS" : "FAIL", v2_v);
//     fprintf(results_file, "[%s]: Balance (%4.2f mV)\n", balance_pass ? "PASS" : "FAIL", balance);
    
//     //display results
//     printf("Cell 1 (%4.2f V): %s\n", v1_v, cell1_pass ? GREEN(PASS) : RED(FAIL));
//     printf("Cell 2 (%4.2f V): %s\n", v2_v, cell2_pass ? GREEN(PASS) : RED(FAIL));
//     printf("Cell diff (%3.0f mV): %s\n", 1000*balance, balance_pass ? GREEN(PASS) : RED(FAIL));

//     //get user input
//     char input = '\0';
//     while ((read(STDIN_FILENO, &input, 1) != 1) && (input == 0)){
//         ;
//     }

//     if(input == 27)
//         return TEST_STATE_TERMINATE;
    
//     if(cell1_pass && cell2_pass && balance_pass){
//         return TEST_STATE_PASSED;
//     }

//     return TEST_STATE_FAILED;
// }