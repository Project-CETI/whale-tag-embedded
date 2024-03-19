//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, 
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, 
//               Joseph DelPreto     (delpreto@csail.mit.edu),
//               Michael Salino-Hugg (msalinohugg@seas.harvard.edu), 
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "state_machine.h"
#include "recovery.h"
#include "utils/config.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
//-----------------------------------------------------------------------------
static int presentState = ST_CONFIG;
// RTC counts
static unsigned int start_rtc_count = 0;
static unsigned int last_reset_rtc_count = 0;
static int current_rtc_count = 0;
static uint32_t burnTimeStart = 0;
// Output file
int g_stateMachine_thread_is_running = 0;
static FILE* stateMachine_data_file = NULL;
static char stateMachine_data_file_notes[256] = "";
static const char* stateMachine_data_file_headers[] = {
  "State To Process",
  "Next State",
  };
static const int num_stateMachine_data_file_headers = 2;

int init_stateMachine() {

  CETI_LOG("Successfully initialized the state machine");
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

    // Set the thread CPU affinity.
    if(STATEMACHINE_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(STATEMACHINE_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("Successfully set affinity to CPU %d", STATEMACHINE_CPU);
      else
        CETI_ERR("Failed to set affinity to CPU %d", STATEMACHINE_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically update state");
    int state_to_process;
    long long global_time_us;
    long long polling_sleep_duration_us;
    g_stateMachine_thread_is_running = 1;
    while (!g_exit) {
      // Acquire timing information for when the next state will begin processing.
      global_time_us = get_global_time_us();
      current_rtc_count = getRtcCount();
      state_to_process = presentState;

      // Process the next state.
      updateStateMachine();

      // Write state information to the data file.
      if(!g_stopAcquisition)
      {
        stateMachine_data_file = fopen(STATEMACHINE_DATA_FILEPATH, "at");
        if(stateMachine_data_file == NULL)
          CETI_LOG("failed to open data output file: %s", STATEMACHINE_DATA_FILEPATH);
        else
        {
          // Write timing information.
          fprintf(stateMachine_data_file, "%lld", global_time_us);
          fprintf(stateMachine_data_file, ",%d", current_rtc_count);
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
        if(polling_sleep_duration_us > 0)
          usleep(polling_sleep_duration_us);
      }
    }
    g_stateMachine_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// State Machine and Controls
// * Details of state machine are documented in the high-level design
//-----------------------------------------------------------------------------
int stateMachine_set_state(wt_state_t new_state){
    //nothing to do
    if(new_state == presentState){
        CETI_LOG("Already in state %s", get_state_str(presentState));
        return 0;
    }

    //actions performed when exit present state
    switch(presentState){
        case ST_REC_SUB:
            activity_led_enable();
            break;

        case ST_BRN_ON:
            #if ENABLE_BURNWIRE
            burnwireOff();
            #endif // ENABLE_BURNWIRE
            break;

        case ST_RETRIEVE:
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_off();
            }
            #endif // ENABLE_RECOVERY
            break;

        default:
            break;
    }

    //actions performed when entering the new state
    switch(new_state){
        case ST_DEPLOY:
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_on();
            }
            #endif // ENABLE_RECOVERY
            break;

        case ST_REC_SUB:   
            activity_led_disable();
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_off();
            }
            #endif // ENABLE_RECOVERY
            break;

        case ST_REC_SURF:
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_on();
            }
            #endif // ENABLE_RECOVERY

            break;

        case ST_BRN_ON:
            burnTimeStart = current_rtc_count;
            #if ENABLE_BURNWIRE
            burnwireOn();
            #endif // ENABLE_BURNWIRE
            break;

        case ST_RETRIEVE:
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_on();
            }
            #endif // ENABLE_RECOVERY
            
            break;

        default:
            break;
    }

    //update state
    CETI_LOG("State transition: %s -> %s\n", get_state_str(presentState), get_state_str(new_state));
    #if ENABLE_RECOVERY
        //set recovery board comment
        recovery_set_comment(get_state_str(new_state));
        
    #endif   
    presentState = new_state;
    return 0;
}

int updateStateMachine() {
    // Deployment sequencer FSM
    switch (presentState) {

    // ---------------- Configuration ----------------
    case (ST_CONFIG): {
        //configure recovery board
        #if ENABLE_RECOVERY
        if (g_config.recovery.enabled) {
            if ((recovery_set_critical_voltage(g_config.critical_voltage_v) == 0) 
                && (recovery_set_aprs_freq_mhz(g_config.recovery.freq_MHz) == 0)
                && (recovery_set_aprs_callsign(&g_config.recovery.callsign) == 0)
                && (recovery_set_aprs_message_recipient(&g_config.recovery.recipient) == 0)
            ) { 
                // send wake message
                char hostname[512];
                gethostname(hostname, 511);
            
                char message[1024];
                snprintf(message, sizeof(message), "CETI %s ready!", hostname);
                recovery_message(message);
                
                char rec_callsign_msg[10];
                char callsign_msg[10];
                callsign_to_str(&g_config.recovery.callsign, callsign_msg);
                callsign_to_str(&g_config.recovery.recipient, rec_callsign_msg);
                CETI_LOG("Recovery configured: %s -> %s @ %7.3f MHz", callsign_msg, rec_callsign_msg, g_config.recovery.freq_MHz);
            } else {
                CETI_ERR("Failed to configure recovery board");
            }
        } else {
            CETI_LOG("Recovery disabled");
            recovery_kill();
        }
        #endif // ENABLE_RECOVERY
        stateMachine_set_state(ST_START);
        break;
    }

    // ---------------- Startup ----------------
    case (ST_START):
        start_rtc_count = getTimeDeploy(); // new v0.5 gets start time from the csv
        last_reset_rtc_count = getRtcCount(); //start_time since last restart (used to keep wifi-enabled)
        CETI_LOG("Deploy Start: %u", start_rtc_count);
        stateMachine_set_state(ST_DEPLOY);
        break;

    // ---------------- Just deployed ----------------
    case (ST_DEPLOY):
        // Waiting for 1st dive
        if((current_rtc_count - start_rtc_count > g_config.timeout_s)) {
            CETI_LOG("TIME OUT!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }

        #if ENABLE_BATTERY_GAUGE
        if ((g_latest_battery_v1_v + g_latest_battery_v2_v < g_config.release_voltage_v)) {
            CETI_LOG("LOW VOLTAGE!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }
        #endif

        #if ENABLE_PRESSURETEMPERATURE_SENSOR
        if ((g_latest_pressureTemperature_pressure_bar > g_config.dive_pressure)
            && (current_rtc_count - last_reset_rtc_count > (WIFI_GRACE_PERIOD_MIN * 60))
        ){
            //disable wifi
            wifi_disable(); 
            wifi_kill();
            // usb_disable();
            activity_led_disable();
            stateMachine_set_state(ST_REC_SUB);// 1st dive after deploy
            break;
        }
        #endif

        break;

    case (ST_REC_SUB):
       if(current_rtc_count - start_rtc_count > g_config.timeout_s) {
            CETI_LOG("TIME OUT!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }
        // Recording while sumberged
        #if ENABLE_BATTERY_GAUGE
        if ((g_latest_battery_v1_v + g_latest_battery_v2_v < g_config.release_voltage_v)) {
            CETI_LOG("LOW VOLTAGE!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }
        #endif //ENABLE_BATTERY_GAUGE

        #if ENABLE_PRESSURETEMPERATURE_SENSOR
        if (g_latest_pressureTemperature_pressure_bar < g_config.surface_pressure) {
            stateMachine_set_state(ST_REC_SURF); // came to surface
            break;
        }
        #endif // ENABLE_PRESSURE_SENSOR

    case (ST_REC_SURF):
        if(current_rtc_count - start_rtc_count > g_config.timeout_s) {
            stateMachine_set_state(ST_BRN_ON);
            break;
        }
        // Recording while at surface, trying to get a GPS fix
        #if ENABLE_BATTERY_GAUGE
        if ((g_latest_battery_v1_v + g_latest_battery_v2_v < g_config.release_voltage_v)) {
            stateMachine_set_state(ST_BRN_ON);
            break;
        }
        #endif

        #if ENABLE_PRESSURETEMPERATURE_SENSOR
        if (g_latest_pressureTemperature_pressure_bar > g_config.dive_pressure) {
            stateMachine_set_state(ST_REC_SUB); //back down...
            break;
        }
        #endif

        break;

    case (ST_BRN_ON):
        // Releasing
            //wait untl burn complete to switch state
        if (current_rtc_count - burnTimeStart > g_config.burn_interval_s) {
        #if ENABLE_BATTERY_GAUGE
            if (g_latest_battery_v1_v + g_latest_battery_v2_v < g_config.critical_voltage_v) {
                stateMachine_set_state(ST_SHUTDOWN);// critical battery

                break;
            }
        #endif

            // update 220109 to leave burnwire on without time limit
            // Leave burn wire on for 20 minutes or until the battery is depleted
            stateMachine_set_state(ST_RETRIEVE);
        }

        break;

    case (ST_RETRIEVE):
        //  Waiting to be retrieved.
        #if ENABLE_BATTERY_GAUGE
        // critical battery
        if (g_latest_battery_v1_v + g_latest_battery_v2_v < g_config.critical_voltage_v) {
            stateMachine_set_state(ST_SHUTDOWN);
            break;
        }
        #endif // ENABLE_BATTERY_GAUGE

        break;

    case (ST_SHUTDOWN):
        //  Shut everything off in an orderly way if battery is critical to
        //  reduce file system corruption risk
        CETI_ERR("!!! Battery critical !!!");
        #if ENABLE_BURNWIRE
        burnwireOff();
        #endif // ENABLE_BURNWIRE
        #if ENABLE_RECOVERY
        if (g_config.recovery.enabled) {
            recovery_off();
        }
        #endif // ENABLE_RECOVERY

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
__attribute__ ((const))
const char* get_state_str(wt_state_t state){
    if((state < ST_CONFIG) || (state > ST_UNKNOWN)) {
        CETI_LOG("presentState is out of bounds. Setting to ST_UNKNOWN. Current value: %d", presentState);
        state = ST_UNKNOWN;
    }
    return state_str[state];
}

