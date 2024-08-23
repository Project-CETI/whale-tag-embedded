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
static int networking_is_enabled = 1;
// RTC counts
static unsigned int start_rtc_s = 0;
static unsigned int burnwire_timeout_start_rtc_s = 0;
static int current_rtc_count_s = 0;
static uint32_t burnwire_started_time_s = 0;
// Output file
int g_stateMachine_thread_is_running = 0;
static FILE* stateMachine_data_file = NULL;
static char stateMachine_data_file_notes[256] = "";
static const char* stateMachine_data_file_headers[] = {
  "State To Process",
  "Next State",
  };
static const int num_stateMachine_data_file_headers = sizeof(stateMachine_data_file_headers)/sizeof(*stateMachine_data_file_headers);

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
      current_rtc_count_s = getRtcCount();
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
          fprintf(stateMachine_data_file, ",%d", current_rtc_count_s);
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
      }
      // Delay to implement a desired sampling rate.
      // Take into account the time it took to process the state.
      polling_sleep_duration_us = STATEMACHINE_UPDATE_PERIOD_US;
      polling_sleep_duration_us -= get_global_time_us() - global_time_us;
      if(polling_sleep_duration_us > 0)
        usleep(polling_sleep_duration_us);
    }
    // Clear the persistent burnwire timeout start time if one exists.
    remove(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH);

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
        // CETI_LOG("Already in state %s", get_state_str(presentState));
        return 0;
    }

    //actions performed when exit present state
    switch(presentState){
        case ST_RECORD_DIVING:
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
                recovery_sleep();
            }
            #endif // ENABLE_RECOVERY
            break;

        default:
            break;
    }

    // Actions performed when entering the new state
    switch(new_state){
        case ST_DEPLOY:
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_wake();
            }
            #endif // ENABLE_RECOVERY
            break;

        case ST_RECORD_DIVING:
            activity_led_disable();
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_sleep();
            }
            #endif // ENABLE_RECOVERY
            // Record this time as the burnwire timeout start time if one has not already been recorded,
            //  since we now know that this is a real deployment.
            if(access(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH, F_OK) == -1) {
              burnwire_timeout_start_rtc_s = getRtcCount();
              CETI_LOG("Starting dive; recording burnwire timeout start time %u", burnwire_timeout_start_rtc_s);
              FILE* file_burnwire_timeout_start_rtc_s = NULL;
              file_burnwire_timeout_start_rtc_s = fopen(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH, "w");
              fprintf(file_burnwire_timeout_start_rtc_s, "%u", burnwire_timeout_start_rtc_s);
              fclose(file_burnwire_timeout_start_rtc_s);
            }
            break;

        case ST_RECORD_SURFACE:
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_wake();
            }
            #endif // ENABLE_RECOVERY

            break;

        case ST_BRN_ON:
            // Turn on the burnwire and record the start time.
            #if ENABLE_BURNWIRE
            burnwireOn();
            burnwire_started_time_s = current_rtc_count_s;
            #endif // ENABLE_BURNWIRE
            // Clear the persistent burnwire timeout start time if one exists.
            remove(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH);
            break;

        case ST_RETRIEVE:
            #if ENABLE_RECOVERY
            if (g_config.recovery.enabled) {
                recovery_wake();
            }
            #endif // ENABLE_RECOVERY
            
            break;

        default:
            break;
    }

    //update state
    CETI_LOG("State transition: %s -> %s\n", get_state_str(presentState), get_state_str(new_state));
    #if ENABLE_RECOVERY
        // send wake message
        char hostname[32];
        gethostname(hostname, 31);

        char comment[41] = {};
        snprintf(comment, 40, "%s %s", hostname, get_state_str(new_state));
        //set recovery board comment
        recovery_set_comment(comment);
        
    #endif   
    presentState = new_state;
    return 0;
}

int updateStateMachine() {
    // Deployment sequencer FSM
    switch (presentState) {

    // ---------------- Configuration ----------------
    case (ST_CONFIG): {
        // configure recovery board
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
        //     recovery_kill();
        }
        #endif // ENABLE_RECOVERY
        
        // Transition to the start state.
        stateMachine_set_state(ST_START);
        break;
    }

    // ---------------- Startup ----------------
    case (ST_START):
        // Record the time of startup (used to keep wifi-enabled)
        start_rtc_s = getRtcCount(); 
        
        // See if a start time for burnwire timeouts has been previously saved.
        //  This would happen if there was an unexpected shutdown during a deployment.
        // If the file is present, use the timestamp it contains.
        // Otherwise, set the timeout start to the current time.
        //  Note that the target time will be at first dive to ensure it's a real deployment,
        //  but will use current time for now in case there is never a dive.
        burnwire_timeout_start_rtc_s = getRtcCount(); // default to the current time
        FILE* file_burnwire_timeout_start_rtc_s = NULL;
        char line[512];
        file_burnwire_timeout_start_rtc_s = fopen(STATEMACHINE_BURNWIRE_TIMEOUT_START_TIME_FILEPATH, "r");
        if(file_burnwire_timeout_start_rtc_s != NULL) {
          CETI_LOG("Loading a previously saved burnwire timeout start time");
          char* fgets_result = fgets(line, 512, file_burnwire_timeout_start_rtc_s);
          fclose(file_burnwire_timeout_start_rtc_s);
          if(fgets_result != NULL)
          {
            unsigned int loaded_start_rtc_s = strtoul(line, NULL, 10);
            if(loaded_start_rtc_s != 0) // will be 0 if the conversion to integer failed
              burnwire_timeout_start_rtc_s = loaded_start_rtc_s;
          }
        }
        CETI_LOG("Using the following burnwire timeout start time: %u", burnwire_timeout_start_rtc_s);

        // Turn off networking if desired.
        #if FORCE_NETWORKS_OFF_ON_START
        wifi_disable();
        wifi_kill();
        bluetooth_kill();
        eth0_disable();
        #endif

        // // Transition to the deployment state.
        // stateMachine_set_state(ST_DEPLOY);

        // Transition to the appropriate recording state.
        #if ENABLE_PRESSURETEMPERATURE_SENSOR
        if(g_pressure->pressure_bar > g_config.dive_pressure) {
            stateMachine_set_state(ST_RECORD_DIVING);
        }
        else {
            stateMachine_set_state(ST_RECORD_SURFACE);
        }
        #else
        stateMachine_set_state(ST_RECORD_DIVING);
        #endif
        break;

    // ---------------- Just deployed ----------------
    // Waiting for 1st dive
    case (ST_DEPLOY):
        if(current_rtc_count_s - burnwire_timeout_start_rtc_s > g_config.timeout_s) {
            CETI_LOG("TIMEOUT!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }

        #if ENABLE_BATTERY_GAUGE
        if ((shm_battery->cell_voltage_v[0] + shm_battery->cell_voltage_v[1] < g_config.release_voltage_v)) {
            CETI_LOG("LOW VOLTAGE!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }
        #endif

        #if ENABLE_PRESSURETEMPERATURE_SENSOR
        if(g_pressure->pressure_bar > g_config.dive_pressure) {
            stateMachine_set_state(ST_RECORD_DIVING); // 1st dive after deploy
            break;
        }
        #endif

        break;

    // Recording while sumberged
    case (ST_RECORD_DIVING):
       // Turn off networking if the grace period has passed.
       if(networking_is_enabled && (current_rtc_count_s - start_rtc_s > (WIFI_GRACE_PERIOD_MIN * 60))) {
         wifi_disable();
         wifi_kill();
         bluetooth_kill();
         eth0_disable();
         // usb_kill();
         activity_led_disable();
         networking_is_enabled = 0;
       }

       // Turn on the burnwire if the timeout has passed since the deployment started.
       if(current_rtc_count_s - burnwire_timeout_start_rtc_s > g_config.timeout_s) {
            CETI_LOG("TIMEOUT!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }

        // Turn on the burnwire if the battery voltage is low.
        #if ENABLE_BATTERY_GAUGE
        if ((shm_battery->cell_voltage_v[0] + shm_battery->cell_voltage_v[1] < g_config.release_voltage_v)) {
            CETI_LOG("LOW VOLTAGE!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }
        #endif //ENABLE_BATTERY_GAUGE

        // Transition state if at the surface.
        #if ENABLE_PRESSURETEMPERATURE_SENSOR
        if (g_pressure->pressure_bar < g_config.surface_pressure) {
            stateMachine_set_state(ST_RECORD_SURFACE); // came to surface
            break;
        }
        #endif // ENABLE_PRESSURE_SENSOR

    // Recording while at surface, trying to get a GPS fix
    case (ST_RECORD_SURFACE):
        // Turn on the burnwire if the timeout has passed since the deployment started.
        if(current_rtc_count_s - burnwire_timeout_start_rtc_s > g_config.timeout_s) {
            CETI_LOG("TIMEOUT!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }

        // Turn on the burnwire if the battery voltage is low.
        #if ENABLE_BATTERY_GAUGE
        if ((shm_battery->cell_voltage_v[0] + shm_battery->cell_voltage_v[1] < g_config.release_voltage_v)) {
            CETI_LOG("LOW VOLTAGE!!! Initializing Burn");
            stateMachine_set_state(ST_BRN_ON);
            break;
        }
        #endif

        // Transition state if diving.
        #if ENABLE_PRESSURETEMPERATURE_SENSOR
        if (g_pressure->pressure_bar > g_config.dive_pressure) {
            stateMachine_set_state(ST_RECORD_DIVING); // back down...
            break;
        }
        #endif

        break;

    // Releasing via the burnwire
    case (ST_BRN_ON):
        // Shutdown if the battery is too low.
        #if ENABLE_BATTERY_GAUGE
        if(shm_battery->cell_voltage_v[0] + shm_battery->cell_voltage_v[1] < g_config.critical_voltage_v) {
            stateMachine_set_state(ST_SHUTDOWN);// critical battery
            break;
        }
        #endif

        // switch state once the burn is complete
        #if ENABLE_BURNWIRE
        if(current_rtc_count_s - burnwire_started_time_s > g_config.burn_interval_s) {
            stateMachine_set_state(ST_RETRIEVE);
        }
        #else
        stateMachine_set_state(ST_RETRIEVE);
        #endif

        break;

    //  Waiting to be retrieved.
    case (ST_RETRIEVE):
        #if ENABLE_BATTERY_GAUGE
        // critical battery
        if (shm_battery->cell_voltage_v[0] + shm_battery->cell_voltage_v[1] < g_config.critical_voltage_v) {
            stateMachine_set_state(ST_SHUTDOWN);
            break;
        }
        #endif // ENABLE_BATTERY_GAUGE

        break;

    //  Shut everything off in an orderly way if battery is critical to
    //  reduce file system corruption risk
    case (ST_SHUTDOWN):
        CETI_ERR("!!! Battery critical !!!");
        #if ENABLE_BURNWIRE
        burnwireOff();
        #endif // ENABLE_BURNWIRE
        #if ENABLE_RECOVERY
        if (g_config.recovery.enabled) {
            // recovery_off();
        }
        #endif // ENABLE_RECOVERY

        // Note that tagMonitor.sh will trigger an actual powerdown once the battery levels drop a bit more.

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
