#include "../commands_internal.h"
#include "../battery.h"

#include <math.h> // for NAN

int batteryCmd_get_current(const char *args) {
    double i_mA = NAN;
    if (getBatteryData(NULL, NULL, &i_mA) != 0) {
        fprintf(g_rsp_pipe, "Error communicating with BMS!!!");
        return -1;
    }
    fprintf(g_rsp_pipe, "%0.3f", i_mA);
    return 0;
}

const CommandDescription battery_subcommand_list[] = {
    {.name = STR_FROM("getCurrent"),    .description = "Read battery current in mA",       .parse=batteryCmd_get_current},
};

const size_t battery_subcommand_list_size = sizeof(battery_subcommand_list)/sizeof(*battery_subcommand_list);