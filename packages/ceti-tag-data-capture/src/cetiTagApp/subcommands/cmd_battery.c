#include "../commands_internal.h"
#include "../battery.h"

#include <math.h>   // for NAN
#include <stdlib.h> // for strtol()

int batteryCmd_get_current(const char *args) {
    double i_mA = NAN;
    if (getBatteryData(NULL, NULL, &i_mA) != 0) {
        fprintf(g_rsp_pipe, "Error communicating with BMS!!!\n");
        return -1;
    }
    fprintf(g_rsp_pipe, "%.3f\n", i_mA);
    return 0;
}

int batteryCmd_get_cell_voltage(const char *args) {
    double v[2] = {NAN, NAN};
    char * end_ptr;
    uint32_t cell_index = strtoul(args, &end_ptr, 0);
    
    if(cell_index > 1 || (end_ptr == args)) {
        fprintf(g_rsp_pipe, "Error invalid cell number.\n");
        fprintf(g_rsp_pipe, "Usage: `battery cellV ( 0 | 1 )`\n");
        return -1;
    }

    if (getBatteryData(&v[0], &v[1], NULL) != 0) {
        fprintf(g_rsp_pipe, "Error communicating with BMS!!!\n");
        return -1;
    }

    fprintf(g_rsp_pipe, "%.2f\n", v[cell_index]);
    return 0;
}

int batteryCmd_get_cell_voltage_1(const char *args) {
    double v_v = NAN;
    if (getBatteryData(&v_v, NULL, NULL) != 0) {
        fprintf(g_rsp_pipe, "Error communicating with BMS!!!\n");
        return -1;
    }
    fprintf(g_rsp_pipe, "%.2f\n", v_v);
    return 0;
}

int batteryCmd_get_cell_voltage_2(const char *args) {
    double v_v = NAN;
    if (getBatteryData(NULL, &v_v, NULL) != 0) {
        fprintf(g_rsp_pipe, "Error communicating with BMS!!!\n");
        return -1;
    }
    fprintf(g_rsp_pipe, "%.2f\n", v_v);
    return 0;
}

int batteryCmd_check_battery(const char *args) {
    double v0_v = NAN;
    double v1_v = NAN;
    double i_mA = NAN;
    if (getBatteryData(&v0_v, &v1_v, &i_mA) != 0) {
        fprintf(g_rsp_pipe, "Error communicating with BMS!!!\n");
        return -1;
    }
    fprintf(g_rsp_pipe, "Battery voltage 1: %.2f V \n", v0_v);
    fprintf(g_rsp_pipe, "Battery voltage 2: %.2f V \n", v1_v);
    fprintf(g_rsp_pipe, "Battery current: %.2f mA \n", i_mA);
    return 0;
}

int batteryCmd_reset(const char *args) {
    resetBattTempFlags();
    fprintf(g_rsp_pipe, "Battery Temperature Flags Reset\n"); // echo it
    return 0;
}

const CommandDescription battery_subcommand_list[] = {
    {.name = STR_FROM("cellV"),     .description = "Read battery cell voltage in V. Usage: `battery cellV ( 0 | 1 )`", .parse=batteryCmd_get_cell_voltage},
    {.name = STR_FROM("current"),   .description = "Read battery current in mA",        .parse=batteryCmd_get_current},
    {.name = STR_FROM("reset"),     .description = "Reset battery temperature flags",   .parse=batteryCmd_reset},
};

const size_t battery_subcommand_list_size = sizeof(battery_subcommand_list)/sizeof(*battery_subcommand_list);