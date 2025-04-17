#include "../commands_internal.h"
#include "../device/bno086.h"
#include "../sensors/imu.h"

int imuCmd_reset(const char *args) {
    bno086_close();
    bno086_open();
    fprintf(g_rsp_pipe, "IMU Resetted and setup\n");
    return 0;
}

const CommandDescription imu_subcommand_list[] = {
    {.name = STR_FROM("reset"), .description = "Reset the IMU", .parse = audioCmd_start},
};

const size_t imu_subcommand_list_size = sizeof(imu_subcommand_list) / sizeof(*imu_subcommand_list);