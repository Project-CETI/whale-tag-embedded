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

#include "utils/str.h"  //for str

#include <stdio.h>      // for FILE
#include <unistd.h>     // for size_t

//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef struct {
    str name;
    const char *description;
    int (*parse)(const char*_String);
} CommandDescription;

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------
extern FILE *g_cmd_pipe;
extern FILE *g_rsp_pipe;


extern const CommandDescription fpga_subcommand_list[];
extern const size_t fpga_subcommand_list_size;

extern const CommandDescription recovery_subcommand_list[];
extern const size_t recovery_subcommand_list_size;

extern const CommandDescription battery_subcommand_list[];
extern const size_t battery_subcommand_list_size;

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
void command_open_response_pipe(void); //in commands.c

//ToDo: these are exposed just to support legacy commands 
int fpga_config(const char *args);
int fpga_reset(const char *args);
int fpga_version(const char *args);

#endif