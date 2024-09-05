#include "../commands_internal.h"
#include "../recovery.h"

static int __recovery_ping(const char *args) {
    //ping recovery board
    if(recovery_ping() == 0){
        fprintf(g_rsp_pipe, "Pong!\n"); // callback received
    } else {
        fprintf(g_rsp_pipe, "Recovery board did not respond\n");
    }
    return 0;
}

const CommandDescription recovery_subcommand_list[] = {
    {.name = STR_FROM("ping"), .description = "Ping the recovery board to verify serial connection", .parse=__recovery_ping},
};

const size_t recovery_subcommand_list_size = sizeof(recovery_subcommand_list)/sizeof(*recovery_subcommand_list);

