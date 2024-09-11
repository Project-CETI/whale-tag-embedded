#include "../commands_internal.h"
#include "../burnwire.h"

int burnwireCmd_on(const char * args) {
    //ToDo: error checking
    burnwireOn();
    fprintf(g_rsp_pipe, "Turned burnwire on\n");
    return 0;
}

int burnwireCmd_off(const char * args) {
    // ToDo: error checking
    burnwireOff();
    fprintf(g_rsp_pipe, "Turned burnwire on\n");
    return 0;
}

const CommandDescription burnwire_subcommand_list[] = {
    {.name = STR_FROM("on"), .description = "Turn on burnwire", .parse=burnwireCmd_on},
    {.name = STR_FROM("off"), .description = "Turn off burnwire", .parse=burnwireCmd_off},
};

const size_t burnwire_subcommand_list_size = sizeof(burnwire_subcommand_list)/sizeof(*burnwire_subcommand_list);