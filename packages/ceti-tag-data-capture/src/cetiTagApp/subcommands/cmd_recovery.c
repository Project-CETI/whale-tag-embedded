#include "../commands_internal.h"
#include "../recovery.h"

#include <stdlib.h> // for strtof()

static int __recoveryCmd_off(const char *args) {
    if (recovery_off() != 0) {
        fprintf(g_rsp_pipe, "Failed to turn off recovery board\n");
        return -1;
    }
    fprintf(g_rsp_pipe, "Recovery board is off\n");
    return 0;
}

static int __recoveryCmd_on(const char *args) {
    if (recovery_off() != 0) {
        fprintf(g_rsp_pipe, "Failed to turn on recovery board\n");
        return -1;
    }
    fprintf(g_rsp_pipe, "Recovery board is on\n");
    return 0;
}

static int __recoveryCmd_ping(const char *args) {
    // ping recovery board
    if (recovery_ping() == 0) {
        fprintf(g_rsp_pipe, "Pong!\n"); // callback received
        return 0;
    } else {
        fprintf(g_rsp_pipe, "Recovery board did not respond\n");
        return -1;
    }
}

static int __recoveryCmd_sendMessage(const char *args) {
    char message[68] = "";
    const char *string_end = NULL;
    const char *string_start = strtoquotedstring(args, &string_end);
    if (string_start == NULL) {
        // quoted string for message not found
        fprintf(g_rsp_pipe, "No message provided to send\n");
        fprintf(g_rsp_pipe, "Usage: recovery message \"Message to send\"\n");
        return -1;
    }
    // strip quotation marks
    string_start++;
    string_end--;

    size_t string_len = string_end - string_start;
    if (string_len > 67 + 1) {
        fprintf(g_rsp_pipe, "[Warning] Oversized message will be truncated\n");
        string_len = 67;
    }
    // clone view to string
    memcpy(message, string_start, string_len);
    message[string_len] = '\0';
    int result = recovery_message(message);
    if (result != 0) {
        fprintf(g_rsp_pipe, "[Error] failed to send recovery message\n");
        return -1;
    }

    fprintf(g_rsp_pipe, "Message: \"%s\" sent\n", message);
    return 0;
}

static int __recoveryCmd_set_frequency(const char *arg) {
    float f_MHz = strtof(arg, NULL);
    if ((f_MHz < 134.0000) || (f_MHz > 174.0000)) {
        fprintf(g_rsp_pipe, "Invalid frequency provided\n");
        fprintf(g_rsp_pipe, "Valid Frequency range 134.0 to 174.0 MHz\n");
        return -1;
    }
    recovery_set_aprs_freq_mhz(f_MHz);
    fprintf(g_rsp_pipe, "APRS frequency set to %.3f MHz\n", f_MHz);
    return 0;
}

static int __recoveryCmd_set_callsign(const char *args) {
    APRSCallsign callsign = {};
    char callsign_str[10];
    if (callsign_try_from_str(&callsign, args, NULL) != 0) {
        fprintf(g_rsp_pipe, "Invalid callsign provided: %s\n", args);
        fprintf(g_rsp_pipe, "Example: recovery setCallsign KC1TUJ-1\n");
        return -1;
    }
    recovery_set_aprs_callsign(&callsign);
    callsign_to_str(&callsign, callsign_str);
    fprintf(g_rsp_pipe, "APRS callsign set to: %s\n", callsign_str);
    return 0;
}

static int __recoveryCmd_set_recipient(const char *args) {
    APRSCallsign callsign = {};
    char callsign_str[10];
    if (callsign_try_from_str(&callsign, args, NULL) != 0) {
        fprintf(g_rsp_pipe, "Invalid callsign provided: %s\n", args);
        fprintf(g_rsp_pipe, "Example: recovery setRecipient KC1TUJ-10\n");
        return -1;
    }
    recovery_set_aprs_message_recipient(&callsign);
    callsign_to_str(&callsign, callsign_str);
    fprintf(g_rsp_pipe, "APRS recipient set to: %s\n", callsign_str);
    return 0;
}

const CommandDescription recovery_subcommand_list[] = {
    {.name = STR_FROM("off"), .description = "Turn off recovery board", .parse = __recoveryCmd_off},
    {.name = STR_FROM("on"), .description = "Turn on  recovery board", .parse = __recoveryCmd_on},
    {.name = STR_FROM("ping"), .description = "Ping the recovery board to verify serial connection", .parse = __recoveryCmd_ping},
    {.name = STR_FROM("message"), .description = "Send a direct message via APRS.", .parse = __recoveryCmd_sendMessage},
    {.name = STR_FROM("setFrequency"), .description = "Sets APRS frequency in MHz", .parse = __recoveryCmd_set_frequency},
    {.name = STR_FROM("setCallsign"), .description = "Sets APRS callsign", .parse = __recoveryCmd_set_callsign},
    {.name = STR_FROM("setRecipient"), .description = "Sets APRS direct message recipient callsign", .parse = __recoveryCmd_set_recipient},
};

const size_t recovery_subcommand_list_size = sizeof(recovery_subcommand_list) / sizeof(*recovery_subcommand_list);
