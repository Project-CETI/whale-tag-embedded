//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "commands.h"

//-----------------------------------------------------------------------------
// Initialize global variables
//-----------------------------------------------------------------------------
FILE *g_cmd_pipe = NULL;
FILE *g_rsp_pipe = NULL;

char g_command[256];

int g_command_thread_is_running = 0;

static char rsp_pipe_path[512];


//-----------------------------------------------------------------------------
// Command-handling logic
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
int init_commands() {
  CETI_LOG("Successfully initialized the command handler [did nothing]");
  return 0;
}

//-----------------------------------------------------------------------------
int handle_command(void) {

  // Declare state used by some of the below commands.
  char fpgaCamResponse[256];

    //-----------------------------------------------------------------------------
    // Part 1 - quit or any other special commands here
    if (!strncmp(g_command, "quit", 4)) {
        CETI_LOG("Received Quit command");
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "Received Quit command - stopping the app\n"); // echo it back
        fclose(g_rsp_pipe);
        CETI_LOG("SETTING EXIT FLAG");
        g_exit = 1;
        return 0;
    }
    
    //-----------------------------------------------------------------------------
    // Part 2 - Client commands
    if (!strncmp(g_command, "dbug", 4)) {
        CETI_LOG("Debug Placeholder is Executing");
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "Running Debug Routine(s)\n"); // echo it back
        fclose(g_rsp_pipe);
        CETI_LOG("Debugged... %d", 1234);
        return 0;
    }

    // if (!strncmp(g_command, "testSerial", 10)) {
    //     CETI_LOG("Testing Recovery Serial Link");
    //     #if ENABLE_RECOVERY
    //     testRecoverySerial();
    //     g_rsp_pipe = fopen(rsp_pipe_path, "w");
    //     fprintf(g_rsp_pipe, "handle_command(): Tested Recovery Serial Link\n");
    //     fclose(g_rsp_pipe);
    //     #else
    //     CETI_LOG("XXXX Recovery is not selected for operation - skipping command XXXX");
    //     #endif
    //     return 0;
    // }

    if (!strncmp(g_command, "bwOn", 4)) {
        CETI_LOG("Turning on burnwire");
        #if ENABLE_BURNWIRE
        burnwireOn();
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Turned burnwire on\n");
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The burnwire is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "bwOff", 5)) {
        CETI_LOG("Turning off burnwire");
        #if ENABLE_BURNWIRE
        burnwireOff();
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Turned burnwire off\n");
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The burnwire is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "rcvryOn", 7)) {
        CETI_LOG("Turning ON power to the Recovery Board");
        #if ENABLE_RECOVERY
        recovery_on();
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Turned Recovery Board ON\n");
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Recovery is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "rcvryOff", 8)) {
        CETI_LOG("Turning OFF power to the Recovery Board");
        #if ENABLE_RECOVERY
        recovery_off();
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Turned Recovery Board OFF\n");
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Recovery is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if ( !strncmp(g_command,"resetIMU",8) ) {
        CETI_LOG("Resetting the IMU");
        #if ENABLE_IMU
        resetIMU();
        g_rsp_pipe = fopen(rsp_pipe_path,"w");
        fprintf(g_rsp_pipe,"handle_command(): Reset the IMU \n");
        fclose(g_rsp_pipe);
        return 0;
        #else
        CETI_LOG("XXXX The IMU is not selected for operation - skipping command XXXX");
        #endif
    }

//    if (!strncmp(g_command, "getRotation", 11)) {
//        CETI_LOG("Retrieving an IMU Vector Rotation input report");
//        #if ENABLE_IMU
//        rotation_t rotation;
//
//        setupIMU();
//        getRotation(&rotation);
//        CETI_LOG("Testing the function 0x%02X 0x%02X", rotation.reportID,
//               rotation.sequenceNum);
//        g_rsp_pipe = fopen(rsp_pipe_path, "w");
//        fprintf(g_rsp_pipe, "handle_command(): Finished getting the Rotation report\n");
//        fclose(g_rsp_pipe);
//        #else
//        CETI_LOG("XXXX The IMU is not selected for operation - skipping command XXXX");
//        #endif
//        return 0;
//    }

    if (!strncmp(g_command, "setupIMU", 8)) {
        CETI_LOG("Setting up the IMU");
        #if ENABLE_IMU
        setupIMU(IMU_ALL_ENABLED);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Finished setting up the IMU\n");
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The IMU is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

//    if (!strncmp(g_command, "learnIMU", 8)) {
//        CETI_LOG("Experimenting with the IMU");
//        #if ENABLE_IMU
//        learnIMU(); // a sandbox function in sensors.c
//        g_rsp_pipe = fopen(rsp_pipe_path, "w");
//        fprintf(g_rsp_pipe, "handle_command(): Finished running IMU experiments\n");
//        fclose(g_rsp_pipe);
//        #else
//        CETI_LOG("XXXX The IMU is not selected for operation - skipping command XXXX");
//        #endif
//        return 0;
//    }

    if (!strncmp(g_command, "initTag", 4)) {

        CETI_LOG("Initializing the Tag");

        if (!init_tag()) {
            CETI_LOG("Tag initialization successful");
            g_rsp_pipe = fopen(rsp_pipe_path, "w");
            fprintf(g_rsp_pipe, "handle_command(): Tag initialization successful\n");
        } else {
            CETI_LOG("XXXX Tag Initialization Failed XXXX");
            g_rsp_pipe = fopen(rsp_pipe_path, "w");
            fprintf(g_rsp_pipe,
                    "handle_command(): Tag initialization failed\n");
        }
        fclose(g_rsp_pipe);
        return 0;
    }

    if (!strncmp(g_command, "configFPGA", 10)) {
        CETI_LOG("Configuring the FPGA");

        #if ENABLE_FPGA
        if (!loadFpgaBitstream("/opt/ceti-tag-data-capture/config/top.bin")) {
            CETI_LOG("FPGA Configuration Succeeded");
            g_rsp_pipe = fopen(rsp_pipe_path, "w");
            fprintf(g_rsp_pipe,
                    "handle_command(): Configuring FPGA Succeeded\n");
        } else {
            CETI_LOG("XXXX FPGA Configuration Failed XXXX");
            g_rsp_pipe = fopen(rsp_pipe_path, "w");
            fprintf(
                g_rsp_pipe,
                "handle_command(): Configuring FPGA Failed - Try Again\n");
        }
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The FPGA is not selected for operation - skipping configuration XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "verFPGA", 7)) {
        CETI_LOG("Querying FPGA Version");
        #if ENABLE_FPGA

        cam(0x10, 0, 0, 0, 0, fpgaCamResponse);

        CETI_LOG("FPGA Version: 0x%02X%02X\n", fpgaCamResponse[4], fpgaCamResponse[5]); // should be FE

        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "FPGA Version: 0x%02X%02X\n", fpgaCamResponse[4], fpgaCamResponse[5]);

        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The FPGA is not selected for operation - skipping configuration XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "checkCAM", 8)) {
        CETI_LOG("Testing CAM Link");
        #if ENABLE_FPGA
        cam(0, 0, 0, 0, 0, fpgaCamResponse);

        g_rsp_pipe = fopen(rsp_pipe_path, "w");

        if (fpgaCamResponse[4] == 0xAA)
            fprintf(g_rsp_pipe, "CAM Link OK\n");
        else
            fprintf(g_rsp_pipe, "CAM Link Failure\n");

        CETI_LOG("camcheck - expect 0xAA: %02X", fpgaCamResponse[4]); // should be FE
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The FPGA is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "startAudioAcq", 13)) {
        CETI_LOG("Starting audio acquisition");
        #if ENABLE_AUDIO
        start_audio_acq();
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Audio acquisition Started\n"); // echo it
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "stopDataAcq", 11)) {
        CETI_LOG("Stopping all data acquisition");
        g_stopAcquisition = 1;
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Data acquisition stopping\n"); // echo it
        fclose(g_rsp_pipe);
        return 0;
    }

    if (!strncmp(g_command, "stopAudioAcq", 12)) {
        CETI_LOG("Stopping audio acquisition");
        #if ENABLE_AUDIO
        stop_audio_acq();
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Audio acquisition Stopped\n"); // echo it
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "resetFPGA", 9)) {
        CETI_LOG("Resetting the FPGA");
        #if ENABLE_FPGA
        gpioSetMode(RESET, PI_OUTPUT);
        gpioWrite(RESET, 0);
        usleep(1000);
        gpioWrite(RESET, 1);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): FPGA Reset Completed\n"); // echo it
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The FPGA is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "simulateAudioOverflow", 21)) {
        CETI_LOG("Simulating an audio buffer overflow");
        g_audio_overflow_detected = 1;
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Simulated audio overflow\n"); // echo it
        fclose(g_rsp_pipe);
        return 0;
    }
    if (!strncmp(g_command, "forceAudioOverflow", 18)) {
        CETI_LOG("Forcing an audio buffer overflow");
        g_audio_force_overflow = 1;
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Forcing an audio overflow\n"); // echo it
        fclose(g_rsp_pipe);
        return 0;
    }

    if (!strncmp(g_command, "setAudioRate_192", 16)) {
        CETI_LOG("Setting audio sampling rate to 192 kHz");
        #if ENABLE_AUDIO
        audio_set_sample_rate(AUDIO_SAMPLE_RATE_192KHZ);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Audio rate set to 192 kHz\n"); // echo it
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "setAudioRate_96", 15)) {
        CETI_LOG("Setting audio sampling rate to 96 kHz");
        #if ENABLE_AUDIO
        audio_set_sample_rate(AUDIO_SAMPLE_RATE_96KHZ);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Audio rate set to 96 kHz\n"); // echo it
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "setAudioRate_48", 15)) {
        CETI_LOG("Setting audio sampling rate to 48 kHz");
        #if ENABLE_AUDIO
        audio_set_sample_rate(AUDIO_SAMPLE_RATE_48KHZ);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Audio rate set to 48 kHz\n"); // echo it
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "setAudioRate_default", 20)) {
        CETI_LOG("Setting audio sampling rate to default (750 Hz)");
        #if ENABLE_AUDIO
        audio_set_sample_rate(AUDIO_SAMPLE_RATE_DEFAULT);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Audio rate set to default (750 Hz)\n"); // echo it
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "resetAudioFIFO", 14)) {
        CETI_LOG("Resetting the audio FIFO");
        #if ENABLE_AUDIO
        reset_audio_fifo();
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Audio FIFO Reset\n"); // echo it
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "checkBatt", 9)) {
        CETI_LOG("Reading battery voltage and current");
        #if ENABLE_BATTERY_GAUGE
        getBatteryData(&g_latest_battery_v1_v, &g_latest_battery_v2_v, &g_latest_battery_i_mA);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "Battery voltage 1: %.2f V \n", g_latest_battery_v1_v);
        fprintf(g_rsp_pipe, "Battery voltage 2: %.2f V \n", g_latest_battery_v2_v);
        fprintf(g_rsp_pipe, "Battery current: %.2f mA \n", g_latest_battery_i_mA);
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The battery gauge is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "checkCell_1", 11)) {
        CETI_LOG("Reading battery cell 1 voltage");
        #if ENABLE_BATTERY_GAUGE
        getBatteryData(&g_latest_battery_v1_v, &g_latest_battery_v2_v, &g_latest_battery_i_mA);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "%.2f\n", g_latest_battery_v1_v);
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The battery gauge is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "checkCell_2", 11)) {
        CETI_LOG("Reading battery cell 2 voltage");
        #if ENABLE_BATTERY_GAUGE
        getBatteryData(&g_latest_battery_v1_v, &g_latest_battery_v2_v, &g_latest_battery_i_mA);
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "%.2f\n", g_latest_battery_v2_v);
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The battery gauge is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "resetBattTemp", 13)) {
        CETI_LOG("Resetting the Battery Temperature Flags");
        resetBattTempFlags();
        g_rsp_pipe = fopen(rsp_pipe_path, "w");
        fprintf(g_rsp_pipe, "handle_command(): Battery Temperature Flags Reset\n"); // echo it
        fclose(g_rsp_pipe);
        return 0;
    }

    if (!strncmp(g_command, "powerdown", 9)) {
        CETI_LOG("Powering down the tag via the FPGA");
        #if ENABLE_FPGA
        g_rsp_pipe = fopen(rsp_pipe_path, "w");

        // now send the FPGA shutdown opcode via CAM
        // opcode 0xF will do a register write on i2c1
        // 0x59 is the 7-bit i2c addr of BMS IC,
        // set register 0 to 0 will turn it off
        // set register 0 to 3 to reactivate

        cam(0x0F, 0xB2, 0x00, 0x00, 0x00, fpgaCamResponse);

        // To complete the shutdown, the Pi must be powered down
        // now by an external process.  Currently the design
        // uses the tagMonitor script to do the Pi shutdown.

        // After the Pi turns off, the FPGA will disable discharging
        // and charging by sending a final i2c message to the BMS chip
        // to pull the plug.

        // A charger connection is required to wake up the tag after this event
        // and charging/discharging needs to subsequently be
        // renabled.

        fprintf(g_rsp_pipe,"handle_command(): Powering the tag down!\n");
        fclose(g_rsp_pipe);
        #else
        CETI_LOG("XXXX The FPGA is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

  // Print available commands.
  g_rsp_pipe = fopen(rsp_pipe_path, "w");
  fprintf(g_rsp_pipe, "\n"); // echo it
  fprintf(g_rsp_pipe, "CETI Tag Electronics Available Commands\n");
  fprintf(g_rsp_pipe,
          "---------------------------------------------------------\n");
  fprintf(g_rsp_pipe, "initTag	    Initialize the Tag\n");

  fprintf(g_rsp_pipe, "configFPGA  Load FPGA bitstream\n");
  fprintf(g_rsp_pipe, "verFPGA     Get FPGA version\n");

  fprintf(g_rsp_pipe, "resetFPGA        Reset FPGA state machines\n");
  fprintf(g_rsp_pipe, "resetAudioFIFO   Reset audio HW FIFO\n");
  fprintf(g_rsp_pipe, "checkCAM         Verify hardware control link\n");

  fprintf(g_rsp_pipe, "stopDataAcq     Stop writing any data to disk\n");

  fprintf(g_rsp_pipe, "startAudioAcq    Start acquiring audio samples from the FIFO\n");
  fprintf(g_rsp_pipe, "stopAudioAcq     Stop acquiring audio samples from  the FIFO\n");
  fprintf(g_rsp_pipe, "setAudioRate_192      Set audio sampling rate to 192 kHz \n");
  fprintf(g_rsp_pipe, "setAudioRate_96       Set audio sampling rate to 96 kHz\n");
  fprintf(g_rsp_pipe, "setAudioRate_48       Set audio sampling rate to 48 kHz \n");
  fprintf(g_rsp_pipe, "setAudioRate_default  Set audio sampling rate to default (750 Hz) \n");

  fprintf(g_rsp_pipe, "resetIMU    Pulse the IMU reset line \n");
  //    fprintf(g_rsp_pipe, "learnIMU    Dev only - sandbox for exploring IMU BNO08x\n");
  fprintf(g_rsp_pipe, "setupIMU    Dev only - bringing up IMU BNO08x\n");
  //    fprintf(g_rsp_pipe, "getRotation Dev only - bringing up IMU BNO08x\n");

  fprintf(g_rsp_pipe, "bwOn        Turn on the burnwire current\n");
  fprintf(g_rsp_pipe, "bwOff       Turn off the burnwire current\n");

  fprintf(g_rsp_pipe, "rcvryOn     Turn on the Recovery Board\n");
  fprintf(g_rsp_pipe, "rcvryOff    Turn off the Recovery Board\n");

  fprintf(g_rsp_pipe, "testSerial  Test Recovery Board serial link\n");

  fprintf(g_rsp_pipe, "checkBatt   Read battery voltages and current\n");
  fprintf(g_rsp_pipe, "checkCell_1 Read battery cell 1 voltage\n");
  fprintf(g_rsp_pipe, "checkCell_2 Read battery cell 2 voltage\n");

  fprintf(g_rsp_pipe, "powerdown   Power down the Tag\n");

  fprintf(g_rsp_pipe, "\n");
  fclose(g_rsp_pipe);

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *command_thread(void *paramPtr) {
  // thread that monitors the cetiCommand pipe and sets a flag if
  // a command is entered. When the FIFO is opened for reading, the external
  // client process needs to open it for writing until this thread
  // advances - that is to say fopen read is blocked waiting to be opened for
  // a write. This is in the background...the main program is still running.

  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_command_thread_tid = gettid();

  // Set the thread CPU affinity.
  if (COMMAND_CPU >= 0) {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(COMMAND_CPU, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", COMMAND_CPU);
    else
      CETI_LOG("XXX Failed to set affinity to CPU %d", COMMAND_CPU);
  }

  //generate pipe paths (relative to process)
  char command_pipe_path[512];
  strncpy(command_pipe_path, g_process_path, sizeof(command_pipe_path) - 1);
  strncat(command_pipe_path, CMD_PIPE_PATH, sizeof(command_pipe_path) - 1);

  strncpy(rsp_pipe_path, g_process_path, sizeof(rsp_pipe_path) - 1);
  strncat(rsp_pipe_path, CMD_PIPE_PATH, sizeof(rsp_pipe_path) - 1);
  CETI_LOG("Starting loop to process commands from %s", command_pipe_path);
  
  // Main loop while application is running.
  g_command_thread_is_running = 1;
  while (!g_exit) {
    // Read commands from the pipe.
    // Note that this will block until a command is sent.
    g_cmd_pipe = fopen(command_pipe_path, "r");
    fgets(g_command, 256, g_cmd_pipe); // get the command
    fclose(g_cmd_pipe);                // close the pipe
    // Process the command if one was present.
    if (strlen(g_command) > 0 && g_command[0] != '\n')
      handle_command();

    // Delay to implement a desired polling rate.
    usleep(COMMAND_POLLING_PERIOD_US);
  }
  g_command_thread_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}
