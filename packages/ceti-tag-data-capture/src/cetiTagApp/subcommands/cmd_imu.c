#include "../commands_internal.h"
#include "../device/bno086.h"
#include "../sensors/imu.h"
#include "../log/imu_log.h"

int imuCmd_reset(const char *args) {
    bno086_close();
    bno086_open();
    fprintf(g_rsp_pipe, "IMU Resetted and setup\n");
    return 0;
}

int imuCmd_force_overflow(const char *args) {
    imu_log_force_overflow();
    return 0;
}

const CommandDescription imu_subcommand_list[] = {
    {.name = STR_FROM("reset"), .description = "Reset the IMU", .parse = imuCmd_reset},
    {.name = STR_FROM("overflow"), .description = "Force the IMU buffer to overflow", .parse = imuCmd_force_overflow},
};

const size_t imu_subcommand_list_size = sizeof(imu_subcommand_list) / sizeof(*imu_subcommand_list);