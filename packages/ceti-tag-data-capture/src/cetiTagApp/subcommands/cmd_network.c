#include "../commands_internal.h"
#include "../utils/power.h"

int networkCmd_off(const char *args) {
    // ToDo: error checking
    networking_disable();
    fprintf(g_rsp_pipe, "Turned all networking off\n");
    return 0;
}

const CommandDescription network_subcommand_list[] = {
    {.name = STR_FROM("off"), .description = "Turn off all networking", .parse = networkCmd_off},
};

const size_t network_subcommand_list_size = sizeof(network_subcommand_list) / sizeof(*network_subcommand_list);