//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "../tests.h"

#include "../tui.h"
#include "../../cetiTagApp/cetiTag.h"

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

TestState test_audio(FILE *pResultsFile){
    char input = 0;
    int channel_pass[3] = {0,0,0};
    const double target = 0.25;

    CetiAudioBuffer *shm_audio;
    sem_t * sem_audio_block;


    // === open audio shared memory ===
    int shm_fd = shm_open(AUDIO_SHM_NAME, O_RDWR, 0444);
    if (shm_fd < 0) {
        fprintf(pResultsFile, "[FAIL]: Audio: Failed to open shared memory\n");
        perror("shm_open");
        return TEST_STATE_FAILED;
    }
    // size to sample size
    if (ftruncate(shm_fd, sizeof(CetiAudioBuffer))){
        fprintf(pResultsFile, "[FAIL]: Audio: Failed to size shared memory\n");
        perror("ftruncate");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }
    // memory map address
    shm_audio = mmap(NULL, sizeof(CetiAudioBuffer), PROT_READ , MAP_SHARED, shm_fd, 0);
    if(shm_audio == MAP_FAILED){
        perror("mmap");
        fprintf(pResultsFile, "[FAIL]: Audio: Failed to map shared memory\n");
        close(shm_fd);
        return TEST_STATE_FAILED;
    }
    close(shm_fd);

    sem_audio_block = sem_open(AUDIO_BLOCK_SEM_NAME, O_RDWR, 0444, 0);
    if(sem_audio_block == SEM_FAILED){
        perror("sem_open");
        munmap(shm_audio, sizeof(CetiAudioBuffer));
        return -1;
    }
    // pthread_t data_acq_thread;
    // AudioConfig audio_config = {
    //     .filter_type = AUDIO_FILTER_WIDEBAND,
    //     .sample_rate = AUDIO_SAMPLE_RATE_96KHZ,
    //     .bit_depth = AUDIO_BIT_DEPTH_16,
    // };

    // test_audio_terminate = 0;
    // if (audio_setup(&audio_config) != 0) {
    //     return TEST_STATE_FAILED;
    // }
    // pthread_mutex_init(&test_audio_swap_lock, NULL);
    // pthread_create(&data_acq_thread, NULL, &test_audio_acq_thread, NULL);
    // test_audio_read_sample = 0;

    for(int i = 0; i < AUDIO_CHANNELS; i++){
        printf("\033[%d;0H", 3 + i*6); printf("CH %d:", i + 1);
        printf("\033[%d;0H", 4 + i*6); printf("  Amplitude:");
        printf("\033[%d;0H", 5 + i*6); printf("  Offset:");
        printf("\033[%d;0H", 6 + i*6); printf("  RMS:");
    }

    // set read location as starting write location
    sem_wait(sem_audio_block);
    // align to audio signal
    size_t offset = (uint8_t*)(shm_audio->data[shm_audio->page].blocks[shm_audio->block]) - shm_audio->data[shm_audio->page].raw; 
    size_t next_sample_index =  (offset + (AUDIO_CHANNELS*sizeof(uint16_t)) - 1) / (AUDIO_CHANNELS*sizeof(uint16_t));
    uint8_t *read_ptr = shm_audio->data[shm_audio->page].sample16[next_sample_index][0];

    usleep(100000); // wait .1 seconds for data to exist

    do {
        // symcronize with block
        sem_wait(sem_audio_block);

        // === analyze sample ===
        uint8_t *end_ptr = (uint8_t*)shm_audio->data[shm_audio->page].blocks[shm_audio->block];
        double min[AUDIO_CHANNELS] = {1.0, 1.0, 1.0};
        double max[AUDIO_CHANNELS] = {-1.0, -1.0, -1.0};
        double avg[AUDIO_CHANNELS] = {0.0, 0.0, 0.0};
        double rms[AUDIO_CHANNELS] = {0.0, 0.0, 0.0};
        double amp[AUDIO_CHANNELS] = {0.0, 0.0, 0.0};
        int sample_count = 0;

        //ToDo: apply HPF

        //check if circular buffer wrapped
        if (end_ptr < read_ptr) {
            //process until end of buffer
            while ( read_ptr < shm_audio->data[1].raw + AUDIO_BUFFER_SIZE_BYTES){
                for(int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++){
                    int16_t sample = ((int16_t)(read_ptr[0]) << 8) | ((int16_t)read_ptr[1]);
                    double sample_f = ((double)sample/0x8000);
                    avg[i_channel] += sample_f;
                    rms[i_channel] += sample_f*sample_f;
                    min[i_channel] = fmin(min[i_channel], sample_f);
                    max[i_channel] = fmax(max[i_channel], sample_f);
                    read_ptr += 2;
                }
                sample_count++;
            }
            read_ptr = shm_audio->data[0].raw;
        }

        //read
        while( read_ptr + AUDIO_CHANNELS*2 <= end_ptr){
            for(int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++){
                int16_t sample = ((int16_t)(read_ptr[0]) << 8) | ((int16_t)read_ptr[1]);
                double sample_f = ((double)sample/0x8000);
                avg[i_channel] += sample_f;
                rms[i_channel] += sample_f*sample_f;
                min[i_channel] = fmin(min[i_channel], sample_f);
                max[i_channel] = fmax(max[i_channel], sample_f);
                read_ptr += 2;
            }
            sample_count++;
        }

        for(int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++){
            avg[i_channel] = avg[i_channel]/((double)sample_count);
            rms[i_channel] = rms[i_channel]/((double)sample_count);
            rms[i_channel] = sqrt(rms[i_channel]);
            amp[i_channel] = fmax(fabs(min[i_channel]), fabs(max[i_channel]));
            channel_pass[i_channel] = amp[i_channel] > target;
        }

        // === display results ===
        for (int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++){
            
            tui_draw_horzontal_bar(amp[i_channel], 1.0, 7, 2 + i_channel*6, tui_get_screen_width() - 7);
            printf("\e[%d;%dH\e[96m|\e[0m\n", 2 + i_channel*6, 7 + (int)(tui_get_screen_width()*target));
            tui_goto(14, 3 + i_channel*6); printf("%6.3f", amp[i_channel]); //Amplitude
            tui_goto(11, 4 + i_channel*6); printf("%6.3f", avg[i_channel]); //Offset
            tui_goto( 8, 5 + i_channel*6); printf("%8.3f dB", 20.0*log(rms[i_channel])); //rms
            tui_goto(1, 6 + i_channel*6); printf("%-20s", channel_pass[i_channel] ? GREEN("PASS") : YELLOW("Pending..."));
        }
    } while((read(STDIN_FILENO, &input, 1) != 1) && input == 0);

    //record results
    int all_pass = 1;
    for(int i = 0; i < 3; i++){
        fprintf(pResultsFile, "Ch%d: %s", i, channel_pass[i] ? "PASS" : "FAIL" );
        all_pass = all_pass && channel_pass[i];
    }

    sem_close(sem_audio_block);
    munmap(shm_audio, sizeof(CetiAudioBuffer));

    if(input == 27)
        return TEST_STATE_TERMINATE;
    
    return all_pass ? TEST_STATE_PASSED : TEST_STATE_FAILED;
}