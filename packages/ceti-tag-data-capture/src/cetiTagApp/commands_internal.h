//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
// This header has definitions that need to acccessible by both commands.c and
// and subcommands/*.c files, but not publicly to the rest of the application
//-----------------------------------------------------------------------------

#ifndef COMMANDS_INTERNAL_H
#define COMMANDS_INTERNAL_H

#include "utils/str.h" //for str

#include <stdio.h>  // for FILE
#include <unistd.h> // for size_t

//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef struct {
    str name;
    const char *description;
    int (*parse)(const char *_String);
} CommandDescription;

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------
extern FILE *g_cmd_pipe;
extern FILE *g_rsp_pipe;

extern const CommandDescription audio_subcommand_list[];
extern const size_t audio_subcommand_list_size;

extern const CommandDescription battery_subcommand_list[];
extern const size_t battery_subcommand_list_size;

extern const CommandDescription burnwire_subcommand_list[];
extern const size_t burnwire_subcommand_list_size;

extern const CommandDescription fpga_subcommand_list[];
extern const size_t fpga_subcommand_list_size;

extern const CommandDescription imu_subcommand_list[];
extern const size_t imu_subcommand_list_size;

extern const CommandDescription mission_subcommand_list[];
extern const size_t mission_subcommand_list_size;

extern const CommandDescription recovery_subcommand_list[];
extern const size_t recovery_subcommand_list_size;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int audioCmd_stop(const char *args);
int audioCmd_start(const char *args);
int audioCmd_reset(const char *args);

int batteryCmd_get_cell_voltage_1(const char *args);
int batteryCmd_get_cell_voltage_2(const char *args);
int batteryCmd_check_battery(const char *args);
int batteryCmd_reset(const char *args);

int burnwireCmd_on(const char *args);
int burnwireCmd_off(const char *args);

// ToDo: these are exposed just to support legacy commands
int fpgaCmd_config(const char *args);
int fpgaCmd_reset(const char *args);
int fpgaCmd_version(const char *args);
int fpgaCmd_checkCam(const char *args);

int imuCmd_reset(const char *args);
#endif