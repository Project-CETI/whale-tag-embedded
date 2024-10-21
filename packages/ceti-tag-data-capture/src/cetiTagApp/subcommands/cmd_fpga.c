#include "../commands_internal.h"
#include "../device/fpga.h"

#include "../utils/logging.h"
#include <ctype.h>

int fpgaCmd_config(const char *args) {
    const char *fpga_bin;
    // skip whitespace
    while (isspace(*args)) {
        args++;
    }
    if (*args == 0) { // no file path provided
        fpga_bin = "/opt/ceti-tag-data-capture/config/top.bin";
        fprintf(g_rsp_pipe, "file %s does not exist!\n", args);
    } else {
        CETI_LOG("checking that %s exists\n", args); // should be FE
        if (access(args, F_OK) != -1) {              // check if file exists
            fpga_bin = args;
        } else {
            fprintf(g_rsp_pipe, "file %s does not exist!\n", args);
            return -1;
        }
    }

    if (wt_fpga_load_bitstream(fpga_bin) == WT_OK) {
        CETI_LOG("FPGA Configuration Succeeded");
        fprintf(g_rsp_pipe, "handle_command(): Configuring FPGA Succeeded\n");
        return 0;

    } else {
        CETI_ERR("XXXX FPGA Configuration Failed XXXX");
        fprintf(g_rsp_pipe, "handle_command(): Configuring FPGA Failed - Try Again\n");
        return -2;
    }
}

int fpgaCmd_version(const char *args) {
    uint16_t version = wt_fpga_get_version();
    CETI_LOG("FPGA Version: 0x%04X\n", version); // should be FE
    fprintf(g_rsp_pipe, "FPGA Version: 0x%04X\n", version);
    return 0;
}

int fpgaCmd_reset(const char *args) {
    wt_fpga_reset();
    fprintf(g_rsp_pipe, "handle_command(): FPGA Reset Completed\n"); // echo it
    return 0;
}

int fpgaCmd_checkCam(const char *args) {
    CETI_LOG("Testing CAM Link");
    char fpgaCamResponse[256];
    wt_fpga_cam(0, 0, 0, 0, 0, fpgaCamResponse);

    if (fpgaCamResponse[4] == 0xAA)
        fprintf(g_rsp_pipe, "CAM Link OK\n");
    else
        fprintf(g_rsp_pipe, "CAM Link Failure\n");

    CETI_LOG("camcheck - expect 0xAA: %02X", fpgaCamResponse[4]); // should be FE
    return 0;
}

const CommandDescription fpga_subcommand_list[] = {
    {.name = STR_FROM("config"), .description = "Load FPGA bitstream: Usage: `fpga config [<path/to/config/file>]'", .parse = fpgaCmd_config},
    {.name = STR_FROM("reset"), .description = "Reset FPGA state machines", .parse = fpgaCmd_reset},
    {.name = STR_FROM("version"), .description = "Query FPGA version", .parse = fpgaCmd_version},
};

const size_t fpga_subcommand_list_size = sizeof(fpga_subcommand_list) / sizeof(*fpga_subcommand_list);
