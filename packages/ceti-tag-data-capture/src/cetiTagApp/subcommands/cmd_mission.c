#include "../commands_internal.h"
#include "../state_machine.h"

static int __missionCmd_setState(const char *args) {
    //get number or identifier 
    wt_state_t state = strtomissionstate(args, NULL);
    if (state == ST_UNKNOWN){
        fprintf(g_rsp_pipe, "Invalid state %s\n", args);
        fprintf(g_rsp_pipe, "Valid states:\n");
        for(int i = 0; i < ST_UNKNOWN; i++){
            fprintf(g_rsp_pipe, "    %s\n", get_state_str(i));
        }
        return -1;
    }

    stateMachine_set_state(state);
    fprintf(g_rsp_pipe, "Mission state set to %s\n", get_state_str(state));
    return 0;
}

static int __missionCmd_pause(const char *args) {
    stateMachine_pause();
    fprintf(g_rsp_pipe, "Mission paused\n");
    return 0;
}

static int __missionCmd_resume(const char *args) {
    wt_state_t state =  stateMachine_get_state();
    stateMachine_resume();
    fprintf(g_rsp_pipe, "Mission resumed from state %s\n", get_state_str(state));
    return 0;
}

static int __missionCmd_restart(const char *args) {
    stateMachine_pause();
    stateMachine_set_state(ST_CONFIG);
    stateMachine_resume();
    return 0;
}

const CommandDescription mission_subcommand_list[] = {
    {.name = STR_FROM("pause"),     .description = "Pauses mission state machine",      .parse=__missionCmd_pause},
    {.name = STR_FROM("resume"),    .description = "Resumes a paused mission",          .parse=__missionCmd_resume},
    {.name = STR_FROM("restart"),   .description = "Restarts mission state machine",    .parse=__missionCmd_restart},
    {.name = STR_FROM("setState"),  .description = "Sets mission state machine state. Usage: `mission setState (<state_identifier> | <state_int>)",  .parse=__missionCmd_setState},
};

const size_t mission_subcommand_list_size = sizeof(mission_subcommand_list)/sizeof(*mission_subcommand_list);