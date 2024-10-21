//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "../tests.h"

#include "../../cetiTagApp/cetiTag.h"
#include "../tui.h"

#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define AUDIO_WINDOW_SIZE_US 5000000
#define AUDIO_WINDOW_SIZE_SAMPLES ((AUDIO_WINDOW_SIZE_US / 1000000) * 96000)

TestState test_audio(FILE *pResultsFile) {
    char input = 0;
    int channel_pass[3] = {0, 0, 0};
    const double target = 0.25;

    CetiAudioBuffer *shm_audio;
    sem_t *sem_audio_block;

    // === open audio shared memory ===
    int shm_fd = shm_open(AUDIO_SHM_NAME, O_RDWR, 0444);
    if (shm_fd < 0) {
        fprintf(pResultsFile, "[FAIL]: Audio: Failed to open shared memory\n");
        perror("shm_open");
        return TEST_STATE_FAILED;
    }
    // size to sample size
    if (ftruncate(shm_fd, sizeof(CetiAudioBuffer))) {
        fprintf(pResultsFile, "[FAIL]: Audio: Failed to size shared memory\n");
        perror("ftruncate");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }
    // memory map address
    shm_audio = mmap(NULL, sizeof(CetiAudioBuffer), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_audio == MAP_FAILED) {
        perror("mmap");
        fprintf(pResultsFile, "[FAIL]: Audio: Failed to map shared memory\n");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }
    close(shm_fd);

    sem_audio_block = sem_open(AUDIO_BLOCK_SEM_NAME, O_RDWR, 0444, 0);
    if (sem_audio_block == SEM_FAILED) {
        perror("sem_open");
        munmap(shm_audio, sizeof(CetiAudioBuffer));
        return TEST_STATE_FAILED;
    }

    for (int i = 0; i < AUDIO_CHANNELS; i++) {
        printf("\033[%d;0H", 3 + i * 6);
        printf("CH %d:", i + 1);
        printf("\033[%d;0H", 4 + i * 6);
        printf("  Amplitude:");
        printf("\033[%d;0H", 5 + i * 6);
        printf("  Offset:");
        printf("\033[%d;0H", 6 + i * 6);
        printf("  RMS:");
    }

    // set read location as starting write location
    sem_wait(sem_audio_block);
    // align to audio signal
    size_t offset = (uint8_t *)(shm_audio->data[shm_audio->page].blocks[shm_audio->block]) - shm_audio->data[shm_audio->page].raw;
    size_t next_sample_index = (offset + (AUDIO_CHANNELS * sizeof(uint16_t)) - 1) / (AUDIO_CHANNELS * sizeof(uint16_t));
    uint8_t *read_ptr = shm_audio->data[shm_audio->page].sample16[next_sample_index][0];
    uint8_t *window_ptr = read_ptr;

    int sample_count = 0;
    usleep(100000); // wait .1 seconds for data to exist

    do {
        // symcronize with block
        sem_wait(sem_audio_block);

        // === analyze sample ===
        uint8_t *end_ptr = (uint8_t *)shm_audio->data[shm_audio->page].blocks[shm_audio->block];
        double min[AUDIO_CHANNELS] = {1.0, 1.0, 1.0};
        double max[AUDIO_CHANNELS] = {-1.0, -1.0, -1.0};
        double sum[AUDIO_CHANNELS] = {0.0, 0.0, 0.0};
        double sq_sum[AUDIO_CHANNELS] = {0.0, 0.0, 0.0};
        double avg[AUDIO_CHANNELS] = {0.0, 0.0, 0.0};
        double rms[AUDIO_CHANNELS] = {0.0, 0.0, 0.0};
        double amp[AUDIO_CHANNELS] = {0.0, 0.0, 0.0};
        // int sample_count = 0;

        // ToDo: apply HPF

        // check if circular buffer wrapped
        if (end_ptr < read_ptr) {
            // process until end of buffer
            while (read_ptr < shm_audio->data[1].raw + AUDIO_BUFFER_SIZE_BYTES) {
                for (int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
                    int16_t sample = ((int16_t)(read_ptr[0]) << 8) | ((int16_t)read_ptr[1]);
                    double sample_f = ((double)sample / 0x8000);
                    sum[i_channel] += sample_f;
                    sq_sum[i_channel] += sample_f * sample_f;
                    min[i_channel] = fmin(min[i_channel], sample_f);
                    max[i_channel] = fmax(max[i_channel], sample_f);
                    read_ptr += 2;
                }
                sample_count++;
            }
            read_ptr = shm_audio->data[0].raw;
        }

        // read
        while (read_ptr + AUDIO_CHANNELS * 2 <= end_ptr) {
            for (int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
                int16_t sample = ((int16_t)(read_ptr[0]) << 8) | ((int16_t)read_ptr[1]);
                double sample_f = ((double)sample / 0x8000);
                sum[i_channel] += sample_f;
                sq_sum[i_channel] += sample_f * sample_f;
                min[i_channel] = fmin(min[i_channel], sample_f);
                max[i_channel] = fmax(max[i_channel], sample_f);
                read_ptr += 2;
            }
            sample_count++;
        }

        // drop old samples
        while (sample_count > AUDIO_WINDOW_SIZE_SAMPLES) {
            for (int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
                int16_t sample = ((int16_t)(window_ptr[0]) << 8) | ((int16_t)window_ptr[1]);
                double sample_f = ((double)sample / 0x8000);
                sum[i_channel] -= sample_f;
                sq_sum[i_channel] -= sample_f * sample_f;
                window_ptr += 2;
                // check for wrap
                if (!(window_ptr < shm_audio->data[1].raw + AUDIO_BUFFER_SIZE_BYTES)) {
                    window_ptr = shm_audio->data[0].raw;
                }
            }
            sample_count--;
        }

        for (int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
            avg[i_channel] = sum[i_channel] / ((double)sample_count);
            rms[i_channel] = sq_sum[i_channel] / ((double)sample_count);
            rms[i_channel] = sqrt(rms[i_channel]);
            amp[i_channel] = fmax(fabs(min[i_channel]), fabs(max[i_channel]));

            // TODO force the operator to play a rhythm game with the hydrophones to pass... jk
            channel_pass[i_channel] |= amp[i_channel] > target;
        }

        // === display results ===
        for (int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++) {
            tui_draw_horzontal_bar(amp[i_channel], 1.0, 7, 3 + i_channel * 6, tui_get_screen_width() - 7);
            printf("\e[%d;%dH\e[96m|\e[0m\n", 3 + i_channel * 6, 7 + (int)(tui_get_screen_width() * target));
            tui_goto(14, 4 + i_channel * 6);
            printf("%6.3f", amp[i_channel]); // Amplitude
            tui_goto(11, 5 + i_channel * 6);
            printf("%6.3f", avg[i_channel]); // Offset
            tui_goto(8, 6 + i_channel * 6);
            printf("%8.3f dB", 20.0 * log(rms[i_channel])); // rms
            tui_goto(1, 7 + i_channel * 6);
            printf("%-20s", channel_pass[i_channel] ? GREEN("PASS") : YELLOW("Pending..."));
        }
    } while ((read(STDIN_FILENO, &input, 1) != 1) && input == 0);

    // record results
    int all_pass = 1;
    for (int i = 0; i < 3; i++) {
        fprintf(pResultsFile, "Ch%d: %s", i, channel_pass[i] ? "PASS" : "FAIL");
        all_pass = all_pass && channel_pass[i];
    }

    sem_close(sem_audio_block);
    munmap(shm_audio, sizeof(CetiAudioBuffer));

    if (input == 27)
        return TEST_STATE_TERMINATE;

    return all_pass ? TEST_STATE_PASSED : TEST_STATE_FAILED;
}