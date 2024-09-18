#include "../commands_internal.h"
#include "../battery.h"
#include "../max17320.h"
#include "../utils/logging.h"

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

typedef struct {
    char *name;
    uint16_t addr;
    uint16_t value;
} NvExpected;

static const NvExpected nv_expected[] = {
    {.name = "NRSENSE",      .addr = 0x1cf, .value = 0x03e8},
    {.name = "NDESIGNCAP",   .addr = 0x1b3, .value = 0x2710},
    {.name = "NPACKCFG",     .addr = 0x1b5, .value = 0xc208},
    {.name = "NNVCFG0",      .addr = 0x1b8, .value = 0x0830},
    {.name = "NNVCFG1",      .addr = 0x1b9, .value = 0x2100},
    {.name = "NNVCFG2",      .addr = 0x1ba, .value = 0x822d},
    {.name = "NUVPRTTH",     .addr = 0x1d0, .value = 0xa002},
    {.name = "NTPRTTH1",     .addr = 0x1d1, .value = 0x280a},
    {.name = "NIPRTTH1",     .addr = 0x1d3, .value = 0x32ce},
    {.name = "NBALTH",       .addr = 0x1d4, .value = 0x0ca0},
    {.name = "NPROTMISCTH",  .addr = 0x1d6, .value = 0x0313},
    {.name = "NPROTCFG",     .addr = 0x1d7, .value = 0x0c08},
    {.name = "NJEITAV",      .addr = 0x1d9, .value = 0xec00},
    {.name = "NOVPRTTH",     .addr = 0x1da, .value = 0xb3a0},
    {.name = "NDELAYCFG",    .addr = 0x1dc, .value = 0x0035},
    {.name = "NODSCCFG",     .addr = 0x1de, .value = 0x4058},
    {.name = "NCONFIG",      .addr = 0x1b0, .value = 0x0290},
    {.name = "NTHERMCFG",    .addr = 0x1ca, .value = 0x71be},
    {.name = "NVEMPTY",      .addr = 0x19e, .value = 0x9659},
    {.name = "NFULLSOCTHR",  .addr = 0x1c6, .value = 0x5005},
};

int batteryCmd_verify(const char * args){
    int incorrect = 0;
    fprintf(g_rsp_pipe, "NV Ram verification not fully implemented\n"); // echo it
    fprintf(g_rsp_pipe, "Nonvoltile RAM Settings:\n"); // echo it
    for (int i = 0; i < sizeof(nv_expected)/sizeof(*nv_expected); i++){
        uint16_t actual;

        //hardware access register
        WTResult result = max17320_read(nv_expected[i].addr, &actual);
        if(result != WT_OK){
            CETI_ERR("BMS device read error: %s\n", wt_strerror(result));
            fprintf(g_rsp_pipe, "BMS device read error: %s\n", wt_strerror(result));
            return -1;
        }

        //assertions
        if (actual != nv_expected[i].value) {
            fprintf(g_rsp_pipe, "%-12s: 0x%04x != 0x%04x !!!!\n", nv_expected[i].name, actual, nv_expected[i].value);
            incorrect++;
        } else {
            fprintf(g_rsp_pipe, "%-12s: 0x%04x  OK!\n", nv_expected[i].name, actual);
        }
    }

    //
    if(incorrect != 0){
        fprintf(g_rsp_pipe, "[WARN]: %d values did not match expected value\n", incorrect);
    } else {
        fprintf(g_rsp_pipe, "OK\n", incorrect);
    }
    return 0;
}

const CommandDescription battery_subcommand_list[] = {
    {.name = STR_FROM("cellV"),     .description = "Read battery cell voltage in V. Usage: `battery cellV ( 0 | 1 )`", .parse=batteryCmd_get_cell_voltage},
    {.name = STR_FROM("current"),   .description = "Read battery current in mA",        .parse=batteryCmd_get_current},
    {.name = STR_FROM("reset"),     .description = "Reset battery temperature flags",   .parse=batteryCmd_reset},
    {.name = STR_FROM("verify"),    .description = "Verify nonvolatile RAM settings",   .parse=batteryCmd_verify},
};

const size_t battery_subcommand_list_size = sizeof(battery_subcommand_list)/sizeof(*battery_subcommand_list);