#include "../commands_internal.h"
#include "../sensors/audio.h"

#include <stdint.h>
#include <stdlib.h>

int audioCmd_reset(const char *args) {
    reset_audio_fifo();
    fprintf(g_rsp_pipe, "Audio FIFO Reset\n"); // echo it
    return 0;
}
int audioCmd_sampleRate(const char *args) {
    uint32_t sample_rate = strtoul(args, NULL, 0);
    switch (sample_rate) {
        case 48:
            audio_set_sample_rate(AUDIO_SAMPLE_RATE_48KHZ);
            break;

        case 96:
            audio_set_sample_rate(AUDIO_SAMPLE_RATE_96KHZ);
            break;

        case 192:
            audio_set_sample_rate(AUDIO_SAMPLE_RATE_192KHZ);
            break;

        default:
            fprintf(g_rsp_pipe, "Error invalid cell number.\n");
            fprintf(g_rsp_pipe, "Usage: `audio sampleRate (48 | 96 | 192)`\n");
            return -1;
    }
    fprintf(g_rsp_pipe, "Audio rate set to %d kHz\n", sample_rate); // echo it
    return 0;
}

int audioCmd_start(const char *args) {
    start_audio_acq();
    fprintf(g_rsp_pipe, "Audio acquisition Started\n"); // echo it
    return 0;
}

int audioCmd_stop(const char *args) {
    stop_audio_acq();
    fprintf(g_rsp_pipe, "Audio acquisition Stopped\n"); // echo it
    return 0;
}

int audioCmd_simulate_overflow(const char *args) {
    g_audio_overflow_detected = 1;
    fprintf(g_rsp_pipe, "Simulated audio overflow\n"); // echo it
    return 0;
}

int audioCmd_force_overflow(const char *args) {
    g_audio_force_overflow = 1;
    fprintf(g_rsp_pipe, "Forcing an audio overflow\n"); // echo it
    return 0;
}

const CommandDescription audio_subcommand_list[] = {
    {.name = STR_FROM("start"), .description = "Start audio acquistion", .parse = audioCmd_start},
    {.name = STR_FROM("stop"), .description = "Stop audio acquistion", .parse = audioCmd_stop},
    {.name = STR_FROM("sampleRate"), .description = "Set audio sampling rate in kHz. Useage: `audio sampleRate (48 | 96 | 192)`", .parse = audioCmd_sampleRate},
    {.name = STR_FROM("reset"), .description = "Reset audio HW FIFO", .parse = audioCmd_reset},
#ifdef DEBUG
    {.name = STR_FROM("forceOverflow"), .description = "Simulate an audio overflow", .parse = audioCmd_simulate_overflow},
    {.name = STR_FROM("simulateOverflow"), .description = "Force an audio overflow", .parse = audioCmd_force_overflow},
#endif
};

const size_t audio_subcommand_list_size = sizeof(audio_subcommand_list) / sizeof(*audio_subcommand_list);