
#include "state_machine.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
static int presentState = ST_CONFIG;
int g_stateMachine_thread_is_running = 0;
static FILE* stateMachine_data_file = NULL;
static char stateMachine_data_file_notes[256] = "";
static const char* stateMachine_data_file_headers[] = {
  "State To Process",
  "Next State",
  };
static const int num_stateMachine_data_file_headers = 2;

int init_stateMachine() {
  CETI_LOG("init_stateMachine(): Successfully initialized the state machine [did nothing]");
  // Open an output file to write data.
  if(init_data_file(stateMachine_data_file, STATEMACHINE_DATA_FILEPATH,
                     stateMachine_data_file_headers,  num_stateMachine_data_file_headers,
                     stateMachine_data_file_notes, "init_stateMachine()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* stateMachine_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_stateMachine_thread_tid = gettid();

    // Main loop while application is running.
    CETI_LOG("stateMachine_thread(): Starting loop to periodically update state");
    int state_to_process;
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_stateMachine_thread_is_running = 1;
    while (!g_exit) {
      // Acquire timing information for when the next state will begin processing.
      global_time_us = get_global_time_us();
      rtc_count = getRtcCount();
      state_to_process = presentState;

      // Process the next state.
      updateStateMachine();

      // Write state information to the data file.
      stateMachine_data_file = fopen(STATEMACHINE_DATA_FILEPATH, "at");
      if(stateMachine_data_file == NULL)
        CETI_LOG("stateMachine_thread(): failed to open data output file: %s", STATEMACHINE_DATA_FILEPATH);
      else
      {
        // Write timing information.
        fprintf(stateMachine_data_file, "%lld", global_time_us);
        fprintf(stateMachine_data_file, ",%d", rtc_count);
        // Write any notes, then clear them so they are only written once.
        fprintf(stateMachine_data_file, ",%s", stateMachine_data_file_notes);
        strcpy(stateMachine_data_file_notes, "");
        // Write the sensor data.
        fprintf(stateMachine_data_file, ",%s", get_state_str(state_to_process));
        fprintf(stateMachine_data_file, ",%s", get_state_str(presentState));
        // Finish the row of data and close the file.
        fprintf(stateMachine_data_file, "\n");
        fclose(stateMachine_data_file);
      }

      // Delay to implement a desired sampling rate.
      // Take into account the time it took to process the state.
      polling_sleep_duration_us = STATEMACHINE_UPDATE_PERIOD_US;
      polling_sleep_duration_us -= get_global_time_us() - global_time_us;
      CETI_LOG("stateMachine_thread(): processing time: %lld us", (get_global_time_us() - global_time_us));
      if(polling_sleep_duration_us > 0)
        usleep(polling_sleep_duration_us);
    }
    g_stateMachine_thread_is_running = 0;
    CETI_LOG("stateMachine_thread(): Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// State Machine and Controls
// * Details of state machine are documented in the high-level design
//-----------------------------------------------------------------------------

int updateStateMachine() {

    // Config file and associated parameters
    static FILE *cetiConfig = NULL;
    char cPress_1[16], cPress_2[16], cVolt_1[16], cVolt_2[16], cTimeOut[16];
    static double d_press_1, d_press_2, d_volt_1, d_volt_2;
    static int timeOut_minutes, timeout_seconds;
    char line[256];
    char *pTemp;

    // timing
    static unsigned int startTime = 0;
    // static int burnTimeStart;
    int rtc_count = getRtcCount();

    // Deployment sequencer FSM
    switch (presentState) {

    // ---------------- Configuration ----------------
    case (ST_CONFIG):
        // Load the deployment configuration
        CETI_LOG("updateState(): Configuring the deployment parameters from %s", CETI_CONFIG_FILE);
        cetiConfig = fopen(CETI_CONFIG_FILE, "r");
        if (cetiConfig == NULL) {
            CETI_LOG("updateState(): XXXX Cannot open configuration file %s", CETI_CONFIG_FILE);
            return (-1);
        }

        while (fgets(line, 256, cetiConfig) != NULL) {

            if (*line == '#')
                continue; // ignore comment lines

            if (!strncmp(line, "P1=", 3)) {
                pTemp = (line + 3);
                strcpy(cPress_1, pTemp);
            }
            if (!strncmp(line, "P2=", 3)) {
                pTemp = (line + 3);
                strcpy(cPress_2, pTemp);
            }
            if (!strncmp(line, "V1=", 3)) {
                pTemp = (line + 3);
                strcpy(cVolt_1, pTemp);
            }
            if (!strncmp(line, "V2=", 3)) {
                pTemp = (line + 3);
                strcpy(cVolt_2, pTemp);
            }
            if (!strncmp(line, "T0=", 3)) {
                pTemp = (line + 3);
                strcpy(cTimeOut, pTemp);
            }
        }

        fclose(cetiConfig);

        // printf("P1 is %s\n",cPress_1);
        // printf("P2 is %s\n",cPress_2);
        // printf("V1 is %s\n",cVolt_1);
        // printf("V2 is %s\n",cVolt_2);
        // printf("Timeout is %s\n",cTimeOut);

        d_press_1 = atof(cPress_1);
        d_press_2 = atof(cPress_2);
        d_volt_1 = atof(cVolt_1);
        d_volt_2 = atof(cVolt_2);
        timeOut_minutes =
            atol(cTimeOut); // the configuration file units are minutes
        timeout_seconds = timeOut_minutes * 60;

        // printf("P1 is %.2f\n",d_press_1);
        // printf("P2 is %.2f\n",d_press_2);
        // printf("V1 is %.2f\n",d_volt_1);
        // printf("V2 is %.2f\n",d_volt_2);
        // printf("Timeout is %d\n",timeOut);

        presentState = ST_START;
        break;

    // ---------------- Startup ----------------
    case (ST_START):
        // Start recording
        #if ENABLE_AUDIO
        start_audio_acq();
        #endif
        startTime = getTimeDeploy(); // new v0.5 gets start time from the csv
        CETI_LOG("updateState(): Deploy Start: %u", startTime);
        recoveryOn();                // turn on Recovery Board
        presentState = ST_DEPLOY;    // underway!
        break;

    // ---------------- Just deployed ----------------
    case (ST_DEPLOY):

        // Waiting for 1st dive
        //	printf("State DEPLOY - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtc_count - startTime), (g_latest_battery_v1_v +
        //g_latest_battery_v2_v));

        #if ENABLE_BATTERY_GAUGE
        if ((g_latest_battery_v1_v + g_latest_battery_v2_v < d_volt_1) ||
            (rtc_count - startTime > timeout_seconds)) {
            burnwireOn();
            presentState = ST_BRN_ON;
            break;
        }
        #endif

        #if ENABLE_PRESSURE_SENSOR
        if (g_latest_pressureTemperature_pressure_bar > d_press_2)
            presentState = ST_REC_SUB; // 1st dive after deploy
        #endif

        break;

    case (ST_REC_SUB):
        // Recording while sumberged
        //	printf("State REC_SUB - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtc_count - startTime), (g_latest_battery_v1_v +
        //g_latest_battery_v2_v));

        #if ENABLE_BATTERY_GAUGE
        if ((g_latest_battery_v1_v + g_latest_battery_v2_v < d_volt_1) ||
            (rtc_count - startTime > timeout_seconds)) {
            // burnTimeStart = rtc_count ;
            burnwireOn();
            presentState = ST_BRN_ON;
            break;
        }
        #endif

        #if ENABLE_PRESSURE_SENSOR
        if (g_latest_pressureTemperature_pressure_bar < d_press_1) {
            presentState = ST_REC_SURF; // came to surface
            break;
        }
        #endif

        recoveryOff();
        break;

    case (ST_REC_SURF):
        // Recording while at surface, trying to get a GPS fix
        //	printf("State REC_SURF - Deployment elapsed time is %d seconds;
        //Battery at %.2f \n", (rtc_count - startTime), (g_latest_battery_v1_v +
        //g_latest_battery_v2_v));

        #if ENABLE_BATTERY_GAUGE
        if ((g_latest_battery_v1_v + g_latest_battery_v2_v < d_volt_1) ||
            (rtc_count - startTime > timeout_seconds)) {
            //	burnTimeStart = rtc_count ;
            burnwireOn();
            presentState = ST_BRN_ON;
            break;
        }
        #endif

        #if ENABLE_PRESSURE_SENSOR
        if (g_latest_pressureTemperature_pressure_bar > d_press_2) {
            recoveryOff();
            presentState = ST_REC_SUB; // back under....
            break;
        }

        if (g_latest_pressureTemperature_pressure_bar < d_press_1) {
            recoveryOn();
        }
        #endif

        break;

    case (ST_BRN_ON):
        // Releasing
        //	printf("State BRN_ON - Deployment elapsed time is %d seconds;
        //  Battery at %.2f \n", (rtc_count - startTime), (g_latest_battery_v1_v +
        //  [1]));

        #if ENABLE_BATTERY_GAUGE
        if (g_latest_battery_v1_v + g_latest_battery_v2_v < d_volt_2) {
            presentState = ST_SHUTDOWN; // critical battery
            break;
        }

        if (g_latest_battery_v1_v + g_latest_battery_v2_v < d_volt_1) {
            presentState = ST_RETRIEVE; // low battery
            break;
        }

        // update 220109 to leave burnwire on without time limit
        // Leave burn wire on for 45 minutes or until the battery is depleted
        // else if (rtc_count - burnTimeStart > (60*45)) {
        //	//burnwireOff();  //leaving it on no time limit -change 220109
        //	nextState = ST_RETRIEVE;
        //}
        #endif

        #if ENABLE_PRESSURETEMPERATURE_SENSOR
        // at surface, turn on the Recovery Board
        if (g_latest_pressureTemperature_pressure_bar < d_press_1) {
            recoveryOn();
        }

        // still under or resubmerged
        if (g_latest_pressureTemperature_pressure_bar > d_press_2) {
            recoveryOff();
        }

        #endif

        break;

    case (ST_RETRIEVE):
        //  Waiting to be retrieved.

        //	printf("State RETRIEVE - Deployment elapsed time is %d seconds;
        //  Battery at %.2f \n", (rtc_count - startTime), (g_latest_battery_v1_v +
        //  g_latest_battery_v2_v));

        #if ENABLE_BATTERY_GAUGE
        // critical battery
        if (g_latest_battery_v1_v + g_latest_battery_v2_v < d_volt_2) {
            presentState = ST_SHUTDOWN;
            break;
        }

        // low battery
        if (g_latest_battery_v1_v + g_latest_battery_v2_v < d_volt_1) {
            burnwireOn(); // redundant, is already on
            recoveryOn();
        }
        #endif

        break;

    case (ST_SHUTDOWN):
        //  Shut everything off in an orderly way if battery is critical to
        //  reduce file system corruption risk
        CETI_LOG("updateState(): !!! Battery critical");
        burnwireOff();
        recoveryOff();

        // 221026 The following system("halt") call has never actually worked - it does
        // not shutoff the Pi as intended. This is being changed so
        // that a new external agent (tagMonitor.sh) will manage the shutdown. As part
        // of this change, the ST_SHUTDOWN state becomes redundant in
        // a sense and may be removed as the architecture firms up.  For the time
        // being just comment out the call.

        //system("halt");

        break;
    }
    return (0);
}

// Helper to convert a state ID to a printable string.
const char* get_state_str(wt_state_t state) {
    if((state < ST_CONFIG) || (state > ST_UNKNOWN)) {
        CETI_LOG("get_state_str(): presentState is out of bounds. Setting to ST_UNKNOWN. Current value: %d", presentState);
        state = ST_UNKNOWN;
    }
    return state_str[state];
}

