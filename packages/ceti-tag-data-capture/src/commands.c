//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "commands.h"

//-----------------------------------------------------------------------------
// Initialize global variables
//-----------------------------------------------------------------------------
FILE *g_cmd_file = NULL;
FILE *g_rsp_file = NULL;

char g_command[256];

int g_command_thread_is_running = 0;

//-----------------------------------------------------------------------------
// Command-handling logic
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
int init_commands() {
    CETI_LOG("init_command(): Successfully initialized the command handler [did nothing]");
    return 0;
}

//-----------------------------------------------------------------------------
int handle_command(void) {

    // Declare state used by some of the below commands.
    char fpgaCamResponse[256];

    //-----------------------------------------------------------------------------
    // Part 1 - quit or any other special commands here
    if (!strncmp(g_command, "quit", 4)) {
        CETI_LOG("handle_command(): Received Quit command");
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "Received Quit command: stopping the app\n"); // echo it back
        fclose(g_rsp_file);
        CETI_LOG("handle_command(): SETTING EXIT FLAG");
        g_exit = 1;
        return 0;
    }
    
    //-----------------------------------------------------------------------------
    // Part 2 - Client commands
    if (!strncmp(g_command, "dbug", 4)) {
        CETI_LOG("handle_command(): Debug Placeholder is Executing");
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "Running Debug Routine(s)\n"); // echo it back
        fclose(g_rsp_file);
        CETI_LOG("handle_command(): Debugged... %d", 1234);
        return 0;
    }

    if (!strncmp(g_command, "testSerial", 10)) {
        CETI_LOG("handle_command(): Testing Recovery Serial Link");
        #if ENABLE_RECOVERY
        testRecoverySerial();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Tested Recovery Serial Link\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Recovery is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "bwOn", 4)) {
        CETI_LOG("handle_command(): Turning on burnwire");
        #if ENABLE_BURNWIRE
        burnwireOn();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Turned burnwire on\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The burnwire is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "bwOff", 5)) {
        CETI_LOG("handle_command(): Turning off burnwire");
        #if ENABLE_BURNWIRE
        burnwireOff();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Turned burnwire off\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The burnwire is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "rcvryOn", 7)) {
        CETI_LOG("handle_command(): Turning ON power to the Recovery Board");
        #if ENABLE_RECOVERY
        recoveryOn();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Turned Recovery Board ON\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Recovery is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "rcvryOff", 8)) {
        CETI_LOG("handle_command(): Turning OFF power to the Recovery Board");
        #if ENABLE_RECOVERY
        recoveryOff();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Turned Recovery Board OFF\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Recovery is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if ( !strncmp(g_command,"resetIMU",8) ) {
        CETI_LOG("handle_command(): Resetting the IMU");
        #if ENABLE_IMU
        resetIMU();
        g_rsp_file = fopen(RSP_FILEPATH,"w");
        fprintf(g_rsp_file,"handle_command(): Reset the IMU \n");
        fclose(g_rsp_file);
        return 0;
        #else
        CETI_LOG("handle_command(): XXXX The IMU is not selected for operation - skipping command XXXX");
        #endif
    }

    if (!strncmp(g_command, "getRotation", 11)) {
        CETI_LOG("handle_command(): Retrieving an IMU Vector Rotation input report");
        #if ENABLE_IMU
        rotation_t rotation;

        setupIMU();
        getRotation(&rotation);
        CETI_LOG("Testing the function 0x%02X 0x%02X", rotation.reportID,
               rotation.sequenceNum);
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Finished getting the Rotation report\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The IMU is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "setupIMU", 8)) {
        CETI_LOG("handle_command(): Setting up the IMU");
        #if ENABLE_IMU
        setupIMU();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Finished setting up the IMU\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The IMU is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "learnIMU", 8)) {
        CETI_LOG("handle_command(): Experimenting with the IMU");
        #if ENABLE_IMU
        learnIMU(); // a sandbox function in sensors.c
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Finished running IMU experiments\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The IMU is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "initTag", 4)) {

        CETI_LOG("handle_command(): Initializing the Tag");

        if (!init_tag()) {
            CETI_LOG("handle_command(): Tag initialization successful");
            g_rsp_file = fopen(RSP_FILEPATH, "w");
            fprintf(g_rsp_file, "handle_command(): Tag initialization successful\n");
        } else {
            CETI_LOG("handle_command(): XXXX Tag Initialization Failed XXXX");
            g_rsp_file = fopen(RSP_FILEPATH, "w");
            fprintf(g_rsp_file,
                    "handle_command(): Tag initialization failed\n");
        }
        fclose(g_rsp_file);
        return 0;
    }

    if (!strncmp(g_command, "configFPGA", 10)) {
        CETI_LOG("handle_command(): Configuring the FPGA");

        #if ENABLE_FPGA
        if (!loadFpgaBitstream()) {
            CETI_LOG("handle_command(): FPGA Configuration Succeeded");
            g_rsp_file = fopen(RSP_FILEPATH, "w");
            fprintf(g_rsp_file,
                    "handle_command(): Configuring FPGA Succeeded\n");
        } else {
            CETI_LOG("handle_command(): XXXX FPGA Configuration Failed XXXX");
            g_rsp_file = fopen(RSP_FILEPATH, "w");
            fprintf(
                g_rsp_file,
                "handle_command(): Configuring FPGA Failed - Try Again\n");
        }
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The FPGA is not selected for operation - skipping configuration XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "verFPGA", 7)) {
        CETI_LOG("handle_command(): Querying FPGA Version");
        #if ENABLE_FPGA

        cam(0x10, 0, 0, 0, 0, fpgaCamResponse);

        CETI_LOG("handle_command(): FPGA Version: 0x%02X%02X\n", fpgaCamResponse[4], fpgaCamResponse[5]); // should be FE

        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "FPGA Version: 0x%02X%02X\n", fpgaCamResponse[4], fpgaCamResponse[5]);

        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The FPGA is not selected for operation - skipping configuration XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "checkCAM", 8)) {
        CETI_LOG("handle_command(): Testing CAM Link");
        #if ENABLE_FPGA
        cam(0, 0, 0, 0, 0, fpgaCamResponse);

        g_rsp_file = fopen(RSP_FILEPATH, "w");

        if (fpgaCamResponse[4] == 0xAA)
            fprintf(g_rsp_file, "CAM Link OK\n");
        else
            fprintf(g_rsp_file, "CAM Link Failure\n");

        CETI_LOG("handle_command(): camcheck - expect 0xAA: %02X", fpgaCamResponse[4]); // should be FE
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The FPGA is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "startAudioAcq", 13)) {
        CETI_LOG("handle_command(): Starting audio acquisition");
        #if ENABLE_AUDIO
        start_audio_acq();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Audio acquisition Started\n"); // echo it
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "stopAudioAcq", 12)) {
        CETI_LOG("handle_command(): Stopping audio acquisition");
        #if ENABLE_AUDIO
        stop_audio_acq();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Audio acquisition Stopped\n"); // echo it
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "resetFPGA", 7)) {
        CETI_LOG("handle_command(): Resetting the FPGA");
        #if ENABLE_FPGA
        gpioSetMode(RESET, PI_OUTPUT);
        gpioWrite(RESET, 0);
        usleep(1000);
        gpioWrite(RESET, 1);
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): FPGA Reset Completed\n"); // echo it
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The FPGA is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "setAudioRate_192", 16)) {
        CETI_LOG("handle_command(): Setting audio sampling rate to 192 kHz");
        #if ENABLE_AUDIO
        setup_audio_192kHz();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Audio rate set to 192 kHz\n"); // echo it
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "setAudioRate_96", 15)) {
        CETI_LOG("handle_command(): Setting audio sampling rate to 96 kHz");
        #if ENABLE_AUDIO
        setup_audio_96kHz();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Audio rate set to 96 kHz\n"); // echo it
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "setAudioRate_48", 15)) {
        CETI_LOG("handle_command(): Setting audio sampling rate to 48 kHz");
        #if ENABLE_AUDIO
        setup_audio_48kHz();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Audio rate set to 48 kHz\n"); // echo it
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "setAudioRate_default", 20)) {
        CETI_LOG("handle_command(): Setting audio sampling rate to default (750 Hz)");
        #if ENABLE_AUDIO
        setup_audio_default();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Audio rate set to default (750 Hz)\n"); // echo it
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "resetAudioFIFO", 14)) {
        CETI_LOG("handle_command(): Resetting the audio FIFO");
        #if ENABLE_AUDIO
        reset_audio_fifo();
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "handle_command(): Audio FIFO Reset\n"); // echo it
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX Audio is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "checkBatt", 9)) {
        CETI_LOG("handle_command(): Reading battery voltage and current");
        #if ENABLE_BATTERY_GAUGE
        getBatteryData(&g_latest_battery_v1_v, &g_latest_battery_v2_v, &g_latest_battery_i_mA);
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "Battery voltage 1: %.2f V \n", g_latest_battery_v1_v);
        fprintf(g_rsp_file, "Battery voltage 2: %.2f V \n", g_latest_battery_v2_v);
        fprintf(g_rsp_file, "Battery current: %.2f mA \n", g_latest_battery_i_mA);
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The battery gauge is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "checkCell_1", 11)) {
        CETI_LOG("handle_command(): Reading battery cell 1 voltage");
        #if ENABLE_BATTERY_GAUGE
        getBatteryData(&g_latest_battery_v1_v, &g_latest_battery_v2_v, &g_latest_battery_i_mA);
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "%.2f\n", g_latest_battery_v1_v);
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The battery gauge is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "checkCell_2", 11)) {
        CETI_LOG("handle_command(): Reading battery cell 2 voltage");
        #if ENABLE_BATTERY_GAUGE
        getBatteryData(&g_latest_battery_v1_v, &g_latest_battery_v2_v, &g_latest_battery_i_mA);
        g_rsp_file = fopen(RSP_FILEPATH, "w");
        fprintf(g_rsp_file, "%.2f\n", g_latest_battery_v2_v);
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The battery gauge is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    if (!strncmp(g_command, "powerdown", 9)) {
        CETI_LOG("handle_command(): Powering down the tag via the FPGA");
        #if ENABLE_FPGA
        g_rsp_file = fopen(RSP_FILEPATH, "w");

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

        fprintf(g_rsp_file,"handle_command(): Powering the tag down!\n");
        fclose(g_rsp_file);
        #else
        CETI_LOG("handle_command(): XXXX The FPGA is not selected for operation - skipping command XXXX");
        #endif
        return 0;
    }

    // Print available commands.
    g_rsp_file = fopen(RSP_FILEPATH, "w");
    fprintf(g_rsp_file, "\n"); // echo it
    fprintf(g_rsp_file, "CETI Tag Electronics Available Commands\n");
    fprintf(g_rsp_file,
            "---------------------------------------------------------\n");
    fprintf(g_rsp_file, "initTag	    Initialize the Tag\n");

    fprintf(g_rsp_file, "configFPGA  Load FPGA bitstream\n");
    fprintf(g_rsp_file, "verFPGA     Get FPGA version\n");

    fprintf(g_rsp_file, "resetFPGA        Reset FPGA state machines\n");
    fprintf(g_rsp_file, "resetAudioFIFO   Reset audio HW FIFO\n");
    fprintf(g_rsp_file, "checkCAM         Verify hardware control link\n");

    fprintf(g_rsp_file, "startAudioAcq    Start acquiring audio samples from the FIFO\n");
    fprintf(g_rsp_file, "stopAudioAcq     Stop acquiring audio samples from  the FIFO\n");
    fprintf(g_rsp_file, "setAudioRate_192      Set audio sampling rate to 192 kHz \n");
    fprintf(g_rsp_file, "setAudioRate_96       Set audio sampling rate to 96 kHz\n");
    fprintf(g_rsp_file, "setAudioRate_48       Set audio sampling rate to 48 kHz \n");
    fprintf(g_rsp_file, "setAudioRate_default  Set audio sampling rate to default (750 Hz) \n");

    fprintf(g_rsp_file, "resetIMU    Pulse the IMU reset line \n");
    fprintf(g_rsp_file, "learnIMU    Dev only - sandbox for exploring IMU BNO08x\n");
    fprintf(g_rsp_file, "setupIMU    Dev only - bringing up IMU BNO08x\n");
    fprintf(g_rsp_file, "getRotation Dev only - bringing up IMU BNO08x\n");

    fprintf(g_rsp_file, "bwOn        Turn on the burnwire current\n");
    fprintf(g_rsp_file, "bwOff       Turn off the burnwire current\n");

    fprintf(g_rsp_file, "rcvryOn     Turn on the Recovery Board\n");
    fprintf(g_rsp_file, "rcvryOff    Turn off the Recovery Board\n");

    fprintf(g_rsp_file, "testSerial  Test Recovery Board serial link\n");

    fprintf(g_rsp_file, "checkBatt   Read battery voltages and current\n");
    fprintf(g_rsp_file, "checkCell_1 Read battery cell 1 voltage\n");
    fprintf(g_rsp_file, "checkCell_2 Read battery cell 2 voltage\n");

    fprintf(g_rsp_file, "powerdown   Power down the Tag\n");

    fprintf(g_rsp_file, "\n");
    fclose(g_rsp_file);

    return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* command_thread(void* paramPtr) {
    // thread that monitors the cetiCommand pipe and sets a flag if
    // a command is entered. When the FIFO is opened for reading, the external
    // client process needs to open it for writing until this thread
    // advances - that is to say fopen read is blocked waiting to be opened for
    // a write. This is in the background...the main program is still running.

    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_command_thread_tid = gettid();
    
    // Set the thread CPU affinity.
    if(COMMAND_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(COMMAND_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("command_thread(): Successfully set affinity to CPU %d", COMMAND_CPU);
      else
        CETI_LOG("command_thread(): XXX Failed to set affinity to CPU %d", COMMAND_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("command_thread(): Starting loop to process commands from %s", CMD_FILEPATH);
    g_command_thread_is_running = 1;
    while (!g_exit) {
      // Read commands from the file.
      g_cmd_file = fopen(CMD_FILEPATH, "r");
      fgets(g_command, 256, g_cmd_file); // get the command
      fclose(g_cmd_file);                     // close the pipe
      // Process the command if one was present.
      if(strlen(g_command) > 0 && g_command[0] != '\n')
      {
        handle_command();
        // Clear the command file so the command is only executed once.
        // This also signals to the user that the command was ingested.
        g_cmd_file = fopen(CMD_FILEPATH, "w");
        fclose(g_cmd_file);
      }

      // Delay to implement a desired polling rate.
      usleep(COMMAND_POLLING_PERIOD_US);
    }
    g_command_thread_is_running = 0;
    CETI_LOG("command_thread(): Done!");
    return NULL;
}