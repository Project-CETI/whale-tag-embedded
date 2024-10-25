//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//     Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "commands.h"          //publice header
#include "commands_internal.h" //semi-private header

#include "battery.h"
#include "burnwire.h"
#include "device/fpga.h"
#include "launcher.h" // for specification of enabled sensors, init_tag(), g_exit, sampling rate, data filepath, and CPU affinity, etc.
#include "sensors/audio.h"
#include "sensors/imu.h"
#include "systemMonitor.h" // for the global CPU assignment variable to update
#include "utils/logging.h"
#include "utils/str.h" //strtoidentifier()
#include "utils/timing.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h> // to set CPU affinity
#include <stdio.h>
#include <string.h>
#include <unistd.h>

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#define ENABLE_LEGACY_COMMANDS 1

//-----------------------------------------------------------------------------
// Private type definitions
//-----------------------------------------------------------------------------
static int __command_quit(const char *args);
static int __command_dbg(const char *args);
static int __command_ping(const char *args);
static int __command_powerdown(const char *args);
static int __command_initTag(const char *args);
static int __command_stopDataAcq(const char *args);
static int __command_startLogging(const char *args);
static int __command_stopLogging(const char *args);
static int handle_audio_command(const char *args);
static int handle_battery_command(const char *args);
static int handle_burnwire_command(const char *args);
static int handle_imu_command(const char *args);
static int handle_fpga_command(const char *args);
static int handle_mission_command(const char *args);
static int handle_recovery_command(const char *args);

static const CommandDescription command_list[] = {
    {.name = STR_FROM("quit"), .description = "Stop the app", .parse = __command_quit}, // special command must be first
    {.name = STR_FROM("dbg"), .description = "Run debug routine", .parse = __command_dbg},
    {.name = STR_FROM("ping"), .description = "Ping cetiTagApp", .parse = __command_ping},
    {.name = STR_FROM("powerdown"), .description = "Power down the Tag", .parse = __command_powerdown},
    {.name = STR_FROM("initTag"), .description = "Initialize the Tag", .parse = __command_initTag},
    {.name = STR_FROM("stopDataAcq"), .description = "Stop acquiring data", .parse = __command_stopDataAcq},
    {.name = STR_FROM("startLogging"), .description = "Start logging collected samples to disk.", .parse = __command_startLogging},
    {.name = STR_FROM("stopLogging"), .description = "Stop logging sensor data to disk.", .parse = __command_stopLogging},

    {.name = STR_FROM("mission"), .description = "Send subcommand for mission state machine", .parse = handle_mission_command},

#if ENABLE_AUDIO
    {.name = STR_FROM("audio"), .description = "Send subcommand for audio", .parse = handle_audio_command},
#if ENABLE_LEGACY_COMMANDS
    {.name = STR_FROM("startAudioAcq"), .description = "Start acquiring audio samples from the FIFO", .parse = audioCmd_start},
    {.name = STR_FROM("stopAudioAcq"), .description = "Stop acquiring audio samples from the FIFO", .parse = audioCmd_stop},
    {.name = STR_FROM("resetAudioFIFO"), .description = "Reset audio HW FIFO", .parse = audioCmd_reset},
#endif
#endif
#if ENABLE_BATTERY_GAUGE
    {.name = STR_FROM("battery"), .description = "Send subcommand for bms", .parse = handle_battery_command},
// now under `battery` command
#if ENABLE_LEGACY_COMMANDS
    {.name = STR_FROM("checkBatt"), .description = "Read battery voltages and current", .parse = batteryCmd_check_battery},
    {.name = STR_FROM("checkCell_1"), .description = "Read battery cell 1 voltage", .parse = batteryCmd_get_cell_voltage_1},
    {.name = STR_FROM("checkCell_2"), .description = "Read battery cell 2 voltage", .parse = batteryCmd_get_cell_voltage_2},
    {.name = STR_FROM("resetBattTemp"), .description = "Reset battery temperature flags", .parse = batteryCmd_reset},
#endif
#endif
#if ENABLE_BURNWIRE
    {.name = STR_FROM("burnwire"), .description = "Send subcommand for burnwire", .parse = handle_burnwire_command},
#if ENABLE_LEGACY_COMMANDS
    {.name = STR_FROM("bwOn"), .description = "Turn on burnwire", .parse = burnwireCmd_on},
    {.name = STR_FROM("bwOff"), .description = "Turn on burnwire", .parse = burnwireCmd_off},
#endif
#endif

#if ENABLE_IMU
    {.name = STR_FROM("imu"), .description = "Send subcommand for imu", .parse = handle_imu_command},
#if ENABLE_LEGACY_COMMANDS
    {.name = STR_FROM("resetIMU"), .description = "Pulse the IMU reset line and setup reports", .parse = imuCmd_reset},
#endif
#endif

#if ENABLE_FPGA
    {.name = STR_FROM("fpga"), .description = "Send subcommand for fpga", .parse = handle_fpga_command},
// now under `fpga` command
#if ENABLE_LEGACY_COMMANDS
    {.name = STR_FROM("resetFPGA"), .description = "Reset FPGA state machines", .parse = fpgaCmd_reset},
    {.name = STR_FROM("verFPGA"), .description = "Query FPGA version", .parse = fpgaCmd_version},
    {.name = STR_FROM("configFPGA"), .description = "Load FPGA bitstream", .parse = fpgaCmd_version},
    {.name = STR_FROM("checkCAM"), .description = "Verify hardware control link", .parse = fpgaCmd_checkCam},
#endif
#endif

#if ENABLE_RECOVERY
    {.name = STR_FROM("recovery"), .description = "Send subcommand for recovery board", .parse = handle_recovery_command},
#endif
};

//-----------------------------------------------------------------------------
// Initialize global variables
//-----------------------------------------------------------------------------
FILE *g_cmd_pipe = NULL;
FILE *g_rsp_pipe = NULL;

char g_command[256];

int g_command_thread_is_running = 0;

static char rsp_pipe_path[512];

//-----------------------------------------------------------------------------
int init_commands() {
    CETI_LOG("Successfully initialized the command handler [did nothing]");
    return 0;
}

//-----------------------------------------------------------------------------
static int __command_quit(const char *args) {
    fprintf(g_rsp_pipe, "Received Quit command - stopping the app\n"); // echo it back
    CETI_LOG("SETTING EXIT FLAG");
    g_exit = 1;
    return 0;
}

static int __command_dbg(const char *args) {
    fprintf(g_rsp_pipe, "Running Debug Routine(s)\n"); // echo it back
    CETI_LOG("Debug routine completed, Shutdown Sequence Testing");
    return 0;
}

static int __command_ping(const char *args) {
    fprintf(g_rsp_pipe, "pong\n");
    CETI_LOG("Ping -> Pong");
    return 0;
}

static int __command_powerdown(const char *args) {
    // now send the FPGA shutdown opcode via CAM
    // opcode 0xF will do a register write on i2c1
    // 0x59 is the 7-bit i2c addr of BMS IC,
    // set register 0 to 0 will turn it off
    // set register 0 to 3 to reactivate

    wt_fpga_shutdown();

    // To complete the shutdown, the Pi must be powered down
    // now by an external process.  Currently the design
    // uses the tagMonitor script to do the Pi shutdown.

    // After the Pi turns off, the FPGA will disable discharging
    // and charging by sending a final i2c message to the BMS chip
    // to pull the plug.

    // A charger connection is required to wake up the tag after this event
    // and charging/discharging needs to subsequently be
    // renabled.
    fprintf(g_rsp_pipe, "handle_command(): Powering the tag down!\n");
    return 0;
}

static int __command_initTag(const char *args) {
    if (!init_tag()) {
        CETI_LOG("Tag initialization successful");
        fprintf(g_rsp_pipe, "handle_command(): Tag initialization successful\n");
    } else {
        CETI_LOG("XXXX Tag Initialization Failed XXXX");
        fprintf(g_rsp_pipe, "handle_command(): Tag initialization failed\n");
    }
    return 0;
}

static int __command_stopDataAcq(const char *args) {
    g_stopAcquisition = 1;
    fprintf(g_rsp_pipe, "Data acquisition stopping\n"); // echo it
    return 0;
}

static int __command_stopLogging(const char *args) {
    g_stopLogging = 1;
    fprintf(g_rsp_pipe, "Logging has been stopped.\n");
    fprintf(g_rsp_pipe, "[WARNING] Data will be lost if not logged by seperate process\n");
    return 0;
}

static int __command_startLogging(const char *args) {
    g_stopLogging = 0;
    fprintf(g_rsp_pipe, "Logging started\n");
    return 0;
}

static int __handle_subcommand(const char *subcmd, const char *args, const CommandDescription *subsub_list, size_t subsub_size) {
    // parse command identifier
    const char *subcommand_end = NULL;
    const char *subcommand = strtoidentifier(args, &subcommand_end);

    // check if a subcommand identifier was found
    if (subcommand != NULL) {
        size_t subcommand_len = (subcommand_end - subcommand);
        for (int i = 0; i < subsub_size; i++) {
            if ((subsub_list[i].name.len == subcommand_len) && (memcmp(subcommand, subsub_list[i].name.ptr, subcommand_len) == 0)) {
                CETI_LOG("Received `%s %s", subcmd, subsub_list[i].name.ptr);
                if (subsub_list[i].parse != NULL) {
                    return subsub_list[i].parse(subcommand_end);
                } else {
                    CETI_WARN("!!!Command does nothing!!!");
                    return 0;
                }
            }
        }
        // subcommand was invalid
        CETI_LOG("Received Invalid `%s` Subcommand", subcmd);
    }
    // subcommand was not found or not valid

    // print recovery subcommand help
    fprintf(g_rsp_pipe, "\n"); // echo it
    fprintf(g_rsp_pipe, "CETI Tag Electronics Available `%s` Subcommands\n", subcmd);
    fprintf(g_rsp_pipe, "---------------------------------------------------------\n");
    for (int i = 0; i < subsub_size; i++) {
        fprintf(g_rsp_pipe, "%-11s %s\n", subsub_list[i].name.ptr, subsub_list[i].description);
    }
    fprintf(g_rsp_pipe, "\n");
    return 0;
}

static int handle_audio_command(const char *args) {
    return __handle_subcommand("audio", args, audio_subcommand_list, audio_subcommand_list_size);
}

static int handle_battery_command(const char *args) {
    return __handle_subcommand("battery", args, battery_subcommand_list, battery_subcommand_list_size);
}

static int handle_burnwire_command(const char *args) {
    return __handle_subcommand("burnwire", args, burnwire_subcommand_list, burnwire_subcommand_list_size);
}

static int handle_imu_command(const char *args) {
    return __handle_subcommand("imu", args, imu_subcommand_list, imu_subcommand_list_size);
}

static int handle_fpga_command(const char *args) {
    return __handle_subcommand("fpga", args, fpga_subcommand_list, fpga_subcommand_list_size);
}

static int handle_mission_command(const char *args) {
    return __handle_subcommand("mission", args, mission_subcommand_list, mission_subcommand_list_size);
}

static int handle_recovery_command(const char *args) {
    return __handle_subcommand("recovery", args, recovery_subcommand_list, recovery_subcommand_list_size);
}

int handle_command(void) {
    // parse command identifier
    const char *command_end = NULL;
    const char *command = strtoidentifier(g_command, &command_end);

    // check if a command identifier was found
    if (command != NULL) {
        size_t command_len = (command_end - command);
        for (int i = 0; i < sizeof(command_list) / sizeof(*command_list); i++) {
            if ((command_list[i].name.len == command_len) && (memcmp(command, command_list[i].name.ptr, command_len) == 0)) {
                g_rsp_pipe = fopen(rsp_pipe_path, "w");
                CETI_LOG("Received Recovery command: %s", command_list[i].name.ptr);
                if (command_list[i].parse != NULL) {
                    int return_val = command_list[i].parse(command_end);
                    fclose(g_rsp_pipe);
                    return return_val;

                } else {
                    CETI_WARN("!!!Command does nothing!!!");
                    fclose(g_rsp_pipe);
                    return 0;
                }
            }
        }
        // Command was invalid
        CETI_LOG("Received Invalid Pipe Command");
    }

    // Print available commands.
    g_rsp_pipe = fopen(rsp_pipe_path, "w");
    fprintf(g_rsp_pipe, "\n"); // echo it
    fprintf(g_rsp_pipe, "CETI Tag Electronics Available Commands\n");
    fprintf(g_rsp_pipe, "---------------------------------------------------------\n");
    for (int i = 0; i < sizeof(command_list) / sizeof(*command_list); i++) {
        if (command_list[i].description != NULL) {
            fprintf(g_rsp_pipe, "%-11s %s\n", command_list[i].name.ptr, command_list[i].description);
        } else {
        }
    }
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

    // generate pipe paths (relative to process)
    char command_pipe_path[512];
    strncpy(command_pipe_path, g_process_path, sizeof(command_pipe_path) - 1);
    strncat(command_pipe_path, CMD_PIPE_PATH, sizeof(command_pipe_path) - 1);

    strncpy(rsp_pipe_path, g_process_path, sizeof(rsp_pipe_path) - 1);
    strncat(rsp_pipe_path, RSP_PIPE_PATH, sizeof(rsp_pipe_path) - 1);
    CETI_LOG("Starting loop to process commands from %s", command_pipe_path);

    // Main loop while application is running.
    g_command_thread_is_running = 1;
    while (!g_exit) {
        // Read commands from the pipe.
        // Note that this will block until a command is sent.
        g_cmd_pipe = fopen(command_pipe_path, "r");
        char *fgets_result = fgets(g_command, 256, g_cmd_pipe); // get the command
        fclose(g_cmd_pipe);                                     // close the pipe
        // Process the command if one was present.
        if (fgets_result != NULL && strlen(g_command) > 0 && g_command[0] != '\n')
            handle_command();

        // Delay to implement a desired polling rate.
        usleep(COMMAND_POLLING_PERIOD_US);
    }
    g_command_thread_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}
