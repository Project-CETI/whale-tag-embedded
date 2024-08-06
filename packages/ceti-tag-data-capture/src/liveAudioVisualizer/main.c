#include "../cetiTagApp/cetiTag.h"

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

int r_page;
int r_block;
CetiAudioBuffer *shm_audio;
sem_t * sem_audio_block;

int main(void){
    // === open audio shared memory ===
    int shm_fd = shm_open(AUDIO_SHM_NAME, O_RDWR, 0444);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }
    // size to sample size
    if (ftruncate(shm_fd, sizeof(CetiAudioBuffer))){
        perror("ftruncate");
        return -1;
    }
    // memory map address
    shm_audio = mmap(NULL, sizeof(CetiAudioBuffer), PROT_READ , MAP_SHARED, shm_fd, 0);
    if(shm_audio == MAP_FAILED){
        perror("mmap");
        return -1;
    }
    close(shm_fd);

    sem_audio_block = sem_open(AUDIO_BLOCK_SEM_NAME, O_RDWR, 0444, 0);
    if(sem_audio_block == SEM_FAILED){
        perror("sem_open");
        return -1;
    }
    
    printf("\033[2J");//clear screen

    //print static screens parts
    for(int i = 0; i < AUDIO_CHANNELS; i++){
        printf("\033[%d;0H", 1 + i*5); printf("CH %d:", i + 1);
        printf("\033[%d;0H", 2 + i*5); printf("  Amplitude:");
        printf("\033[%d;0H", 3 + i*5); printf("  Offset:");
        printf("\033[%d;0H", 4 + i*5); printf("  RMS:");
    }

    // set read location as starting write location
    sem_wait(sem_audio_block);
    r_page = shm_audio->page;
    r_block = shm_audio->block;
    // align to audio signal
    size_t offset = (uint8_t*)(shm_audio->data[r_page].blocks[r_page]) - shm_audio->data[r_page].raw; 
    size_t next_sample_index =  (offset + (AUDIO_CHANNELS*sizeof(uint16_t)) - 1) / (AUDIO_CHANNELS*sizeof(uint16_t));
    uint8_t *read_ptr = shm_audio->data[r_page].sample16[next_sample_index][0];

    usleep(1000000); //wait for atleast 1 second of data
    // main loop 
    while(1){
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
        }

        // === display results ===
        for (int i_channel = 0; i_channel < AUDIO_CHANNELS; i_channel++){
            printf("\033[%d;14H", 2 + i_channel*5); printf("%6.3f", amp[i_channel]); //Amplitude
            printf("\033[%d;11H", 3 + i_channel*5); printf("%6.3f", avg[i_channel]); //Offset
            printf("\033[%d;8H", 4 + i_channel*5); printf("%8.3f dB", 20.0*log(rms[i_channel])); //rms
        }

        // === sleep ===
        usleep(100000);
    }
}