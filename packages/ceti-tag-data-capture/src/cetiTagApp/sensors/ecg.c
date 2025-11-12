
//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "ecg.h"

#include "../utils/memory.h"
#include "../utils/thread_error.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>

#define SLEEPY_ECG 0

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// ECG Note Flags
#define ECG_NOTE_RESTARTED (1 << 0)
#define ECG_NOTE_NEW_LOG (1 << 1)
#define ECG_NOTE_ZEROS (1 << 2)
#define ECG_NOTE_TIMEOUT (1 << 3)
#define ECG_NOTE_MAYBE_INVALID (1 << 4)
#define ECG_NOTE_ERRORS (1 << 5)

// Global/static variables
int g_ecg_thread_getData_is_running = 0;
int g_ecg_thread_writeData_is_running = 0;
static char ecg_data_filepath[100];
static FILE *ecg_data_file = NULL;
static const char *ecg_data_file_headers[] = {
    "Sample Index",
    "ECG",
    "Leads-Off-P",
    "Leads-Off-N",
};
static const int num_ecg_data_file_headers = sizeof(ecg_data_file_headers) / sizeof(*ecg_data_file_headers);

static volatile int ecg_buffer_select_toWrite = 0; // which buffer will be flushed to the output file

static uint8_t ecg_note_flags[ECG_NUM_BUFFERS][ECG_BUFFER_LENGTH] = {0};

static CetiEcgBuffer *shm_ecg; // share memory of other processes to directly access samples
static sem_t *sem_ecg_sample;  // semaphore for other processes to sync with new sample becoming available
static sem_t *sem_ecg_page;    // semaphore for other processes to sync with new pages becoming available

int init_ecg() {
    char err_str[512];
    int t_result = THREAD_OK;
    // Initialize the GPIO expander and the ADC.
    if (init_ecg_electronics() < 0) {
        CETI_ERR("Unknown hardware error");
        t_result |= THREAD_ERR_HW;
    }

    // Create shared memory
    shm_ecg = create_shared_memory_region(ECG_SHM_NAME, sizeof(CetiEcgBuffer));
    if (shm_ecg == NULL) {
        CETI_ERR("Failed to create shared memory region: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SHM_FAILED;
    }

    // setup semaphore
    sem_ecg_sample = sem_open(ECG_SAMPLE_SEM_NAME, O_CREAT, 0644, 0);
    if (sem_ecg_sample == SEM_FAILED) {
        CETI_ERR("Failed to create block ready semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }

    sem_ecg_page = sem_open(ECG_PAGE_SEM_NAME, O_CREAT, 0644, 0);
    if (sem_ecg_page == SEM_FAILED) {
        CETI_ERR("Failed to create block ready semaphore: %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_SEM_FAILED;
    }

    shm_ecg->lod_enabled = ENABLE_ECG_LOD;

    // Open an output file to write data.
    if (init_ecg_data_file(1) < 0) {
        CETI_ERR("Failed to open/create an output data file: " AUDIO_STATUS_FILEPATH ": %s", strerror_r(errno, err_str, sizeof(err_str)));
        t_result |= THREAD_ERR_DATA_FILE_FAILED;
    }

    return t_result;
}

int init_ecg_electronics() {
#if ENABLE_ECG_LOD
    ecg_lod_init();
#endif

    // Set up and configure the ADC.
    if (ecg_adc_setup(ECG_I2C_BUS) < 0)
        return -1;
    ecg_adc_set_voltage_reference(ECG_ADC_VREF_EXTERNAL); // ECG_ADC_VREF_EXTERNAL or ECG_ADC_VREF_INTERNAL
    ecg_adc_set_gain(ECG_ADC_GAIN_ONE);                   // ECG_ADC_GAIN_ONE or ECG_ADC_GAIN_FOUR
    ecg_adc_set_data_rate(1000);                          // 20, 90, 330, or 1000
    ecg_adc_set_conversion_mode(ECG_ADC_MODE_CONTINUOUS); // ECG_ADC_MODE_CONTINUOUS or ECG_ADC_MODE_SINGLE_SHOT
    ecg_adc_set_channel(ECG_ADC_CHANNEL_ECG);
    // Start continuous conversion (or a single reading).
    ecg_adc_start();

    CETI_LOG("Successfully initialized the ECG electronics");
    CETI_LOG("ECG data-ready pin: %d", ECG_ADC_DATA_READY_PIN);

    return 0;
}

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// Determine a new ECG data filename that does not already exist, and open a file for it.
int init_ecg_data_file(int restarted_program) {
    // Append a number to the filename base until one is found that doesn't exist yet.
    int data_file_postfix_count = 0;
    int data_file_exists = 0;
    do {
        sprintf(ecg_data_filepath, "%s_%02d.csv", ECG_DATA_FILEPATH_BASE, data_file_postfix_count);
        data_file_exists = (access(ecg_data_filepath, F_OK) != -1);
        data_file_postfix_count++;
    } while (data_file_exists);

    // Open the new file.
    int init_data_file_success = init_data_file(ecg_data_filepath,
                                                ecg_data_file_headers, num_ecg_data_file_headers,
                                                NULL,
                                                "init_ecg_data_file()");
    ecg_note_flags[shm_ecg->page][0] |= ECG_NOTE_RESTARTED;

    // Change the note from restarted to new file if this is not the first initialization.
    if (!restarted_program) {
        ecg_note_flags[shm_ecg->page][0] |= ECG_NOTE_NEW_LOG;
    }

    return init_data_file_success;
}

//-----------------------------------------------------------------------------
// Thread to acquire data into a rolling buffer
//-----------------------------------------------------------------------------
void *ecg_thread_getData(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_ecg_thread_getData_tid = gettid();

    if ((shm_ecg == NULL) || (sem_ecg_page == SEM_FAILED) || (sem_ecg_sample == SEM_FAILED)) {
        CETI_ERR("Thread started without neccesary memory resources");
        // Clean up.
        ecg_adc_cleanup();
        munmap(shm_ecg, sizeof(CetiEcgBuffer));
        sem_close(sem_ecg_sample);
        sem_close(sem_ecg_page);

        shm_unlink(ECG_SHM_NAME);
        sem_unlink(ECG_SAMPLE_SEM_NAME);
        sem_unlink(ECG_PAGE_SEM_NAME);

        g_ecg_thread_getData_is_running = 0;
        CETI_LOG("Terminated!");
        return NULL;
    }

    // Main loop while application is running.
    CETI_LOG("Starting loop to periodically acquire data");
    g_ecg_thread_getData_is_running = 1;

    // Continuously poll the ADC and the leads-off detection output.
    long long prev_ecg_adc_latest_reading_global_time_us = 0;
    shm_ecg->sample = 0;
    long long sample_index = 0;
    long consecutive_zero_ecg_count = 0;
    long instantaneous_sampling_period_us = 0;
    int first_sample = 1;
    int64_t start_time_ms = get_monotonic_time_ms();
    while (!g_stopAcquisition) {
        // wait for data to be ready
        if (ecg_adc_read_data_ready() != 0) {
            // don't worry about sleeping;
            //  usleep(100);

            // ToDo: does not implement timeout check like non-sleepy code
            continue; // continue used to guarentee outer loop exit conditions are checked and respected
        }

        // Store the new data sample and its timestamp.
        CetiEcgSample *current_ecg_sample = &shm_ecg->data[shm_ecg->page][shm_ecg->sample];
        WTResult adc_status = current_ecg_sample->error = ecg_adc_raw_read_data(&current_ecg_sample->ecg_reading);
        current_ecg_sample->sys_time_us = get_global_time_us();

        // Update the previous timestamp, for checking whether new data is available.
        instantaneous_sampling_period_us = current_ecg_sample->sys_time_us - prev_ecg_adc_latest_reading_global_time_us;
        prev_ecg_adc_latest_reading_global_time_us = current_ecg_sample->sys_time_us;

#if ENABLE_ECG_LOD
        // Read the GPIO expander for the latest leads-off detection.
        // Assume it's fast enough that the ECG sample timestamp is close enough to this leads-off timestamp.
        WTResult lod_status = ecg_get_latest_leadsOff_detections(
            &current_ecg_sample->leadsOff_reading_p,
            &current_ecg_sample->leadsOff_reading_n);

        if (current_ecg_sample->error == WT_OK) {
            current_ecg_sample->error = lod_status;
        }
#endif

        // Read the RTC.
        current_ecg_sample->rtc_time_s = getRtcCount();

        // Update indexes.
        current_ecg_sample->sample_index = sample_index;
        sample_index++;

        ecg_note_flags[shm_ecg->page][shm_ecg->sample] = 0;
        /* MSH: Possible performance improvements:
         * 1) Reserve sample processing (i.e. conversion to strings) for
         * buffer write operation.
         * 2) strcat() requires iteration over the existing string every call.
         * Consider tracking end of current notes pointer.
         */

        // Check if there was an error reading from the ADC.
        // Note that the sample will already be set to ECG_INVALID_PLACEHOLDER
        //  if there was an explicit I2C error communicating with the ADC.
        // But if the ECG board is not connected, then the ADC will seemingly
        //  always have data ready and always return 0.
        // So also check if the ADC returned exactly 0 many times in a row.
        if (adc_status != WT_OK) {
            ecg_note_flags[shm_ecg->page][shm_ecg->sample] |= ECG_NOTE_ERRORS;
            char err_str[512];
            wt_strerror_r(adc_status, err_str, sizeof(err_str));
            CETI_DEBUG("ADC encountered an ERROR(%s)", err_str);
        }

        if (current_ecg_sample->ecg_reading == 0) {
            consecutive_zero_ecg_count++;
        } else {
            consecutive_zero_ecg_count = 0;
        }

        if (consecutive_zero_ecg_count > ECG_ZEROCOUNT_THRESHOLD) {
            ecg_note_flags[shm_ecg->page][shm_ecg->sample] |= ECG_NOTE_ZEROS;
            CETI_DEBUG("ADC returned %ld zero readings in a row", consecutive_zero_ecg_count);
        }

        // Check if it took longer than expected to receive the sample (from the ADC and the GPIO expander combined).
        if (instantaneous_sampling_period_us > ECG_SAMPLE_TIMEOUT_US && !first_sample) {
            ecg_note_flags[shm_ecg->page][shm_ecg->sample] |= ECG_NOTE_TIMEOUT;
            CETI_DEBUG("XXX Reading a sample took %ld us", instantaneous_sampling_period_us);
        }

        // Update state.
        first_sample = 0;

        // If the ADC or the GPIO expander had an error,
        //  wait a bit and then try to reconnect to them.
        uint8_t should_reinitilize = (ECG_NOTE_ERRORS | ECG_NOTE_ZEROS | ECG_NOTE_TIMEOUT) & ecg_note_flags[shm_ecg->page][shm_ecg->sample];
        if (should_reinitilize && !g_stopAcquisition) {
            ecg_note_flags[shm_ecg->page][shm_ecg->sample] |= ECG_NOTE_MAYBE_INVALID;
            usleep(1000000);
            init_ecg_electronics();
            usleep(10000);
            consecutive_zero_ecg_count = 0;
            first_sample = 1;
            continue;
        }

        // Advance the buffer index.
        // If the buffer has filled, switch to the other buffer
        //   (this will also trigger the writeData thread to write the previous buffer to a file).
        shm_ecg->sample++;
        if (shm_ecg->sample == ECG_BUFFER_LENGTH) {
            shm_ecg->sample = 0;
            int next_page = (shm_ecg->page + 1) % ECG_NUM_BUFFERS;
            if (next_page == ecg_buffer_select_toWrite) {
                CETI_ERR("***OVERFLOW*** ECG buffer overflow detected.");
                /* ToDo: handle this type of overflow */
            }
            shm_ecg->page = next_page;
            sem_post(sem_ecg_page);
        }
        sem_post(sem_ecg_sample);

        // Note: The below sleep was commented since it seems to be associated with periodically varying
        //       sampling rates and with artifacts in the ECG spectrogram.  This will be futher investigated,
        //       but for now it is removed to improve signal integrity.
        // Note: Sleeping for 75% of the sample interval seems to reduce utilization of this CPU core from
        //       approximately 85% to 9%, and seems to reduce overall power consumption by approximately 10%.
        // // sleep duration shortened to 75% of sample interval to ensure ADC config still dictates sampling interval
#if SLEEPY_ECG
        if (ecg_adc_read_data_ready()) {
            continue;
        }
        int64_t elapsed_time = (get_monotonic_time_ms() - prev_ecg_adc_latest_reading_global_time_us);
        if ((ECG_SAMPLING_PERIOD_US * 75 / 100 - elapsed_time) > 0) {
            usleep(ECG_SAMPLING_PERIOD_US * 75 / 100 - elapsed_time);
        }
#endif // SLEEPY_ECG
    }
    // Print the duration and the sampling rate.
    long long duration_ms = get_monotonic_time_ms() - start_time_ms;
    CETI_LOG("Average rate %0.2f Hz (%lld samples in %lld ms)",
             1000.0 * (float)sample_index / (float)duration_ms,
             sample_index, duration_ms);

    // Clean up.
    ecg_adc_cleanup();
    ecg_adc_powerDown();

    // wait for ecg writing thread to stop before freeing up resources
    threadManager_join_thread(ACQ_THREAD_ECG_LOG);

    munmap(shm_ecg, sizeof(CetiEcgBuffer));
    sem_close(sem_ecg_sample);
    sem_close(sem_ecg_page);

    shm_unlink(ECG_SHM_NAME);
    sem_unlink(ECG_SAMPLE_SEM_NAME);
    sem_unlink(ECG_PAGE_SEM_NAME);
    shm_ecg = NULL;

    g_ecg_thread_getData_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Thread to write data from the rolling buffer to a file
//-----------------------------------------------------------------------------
static void __ecg_sample_to_csv(const CetiEcgSample *sample, uint8_t note_flags) {
    // Write timing information.
    fprintf(ecg_data_file, "%lu", sample->sys_time_us);
    fprintf(ecg_data_file, ",%u", sample->rtc_time_s);
    // Write any notes.
    fprintf(ecg_data_file, ",");
    if (ECG_NOTE_RESTARTED & note_flags) {
        fprintf(ecg_data_file, "Restarted! | ");
    }
    if (ECG_NOTE_NEW_LOG & note_flags) {
        fprintf(ecg_data_file, "New log file! | ");
    }
    // Note if a device error occured
    if (sample->error != WT_OK) {
        char err_str[512];
        fprintf(ecg_data_file, "ERROR(%s) | ", wt_strerror_r(sample->error, err_str, sizeof(err_str)));
    }

    if (ECG_NOTE_ZEROS & note_flags) {
        fprintf(ecg_data_file, "ADC ZEROS | ");
    }

    if (ECG_NOTE_TIMEOUT & note_flags) {
        fprintf(ecg_data_file, "TIMEOUT | ");
    }
    if (ECG_NOTE_MAYBE_INVALID & note_flags) {
        fprintf(ecg_data_file, "INVALID? | ");
    }

    // Write the sensor data.
    fprintf(ecg_data_file, ",%lu", sample->sample_index);
    fprintf(ecg_data_file, ",%d", sample->ecg_reading);
#if ENABLE_ECG_LOD
    fprintf(ecg_data_file, ",%u", sample->leadsOff_reading_p);
    fprintf(ecg_data_file, ",%u", sample->leadsOff_reading_n);
#else
    fprintf(ecg_data_file, ",,");
#endif
    // Finish the row of data.
    fprintf(ecg_data_file, "\n");
}

void *ecg_thread_writeData(void *paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_ecg_thread_writeData_tid = gettid();

    // Main loop while application is running.
    CETI_LOG("Starting loop to write data as it is acquired");
    g_ecg_thread_writeData_is_running = 1;

    // Continuously wait for new data and then write it to the file.
    while (!g_stopAcquisition) {
        // Wait for new data to be in the buffer.
        int nv_ecg_buffer_select_toWrite = ecg_buffer_select_toWrite;
        if (shm_ecg->page == nv_ecg_buffer_select_toWrite) {
            usleep(250000);
            continue;
        }

        if (!g_stopLogging) {
            // Write the last buffer to a file.
            long ecg_data_file_size_b = 0;
            ecg_data_file = fopen(ecg_data_filepath, "at");
            if (ecg_data_file == NULL) {
                CETI_LOG("failed to open data output file: %s", ecg_data_filepath);
                init_ecg_data_file(0);
                continue;
            }

            // Write the buffer data to the file.
            for (int ecg_buffer_index_toWrite = 0; ecg_buffer_index_toWrite < ECG_BUFFER_LENGTH; ecg_buffer_index_toWrite++) {
                CetiEcgSample *current_sample = &shm_ecg->data[nv_ecg_buffer_select_toWrite][ecg_buffer_index_toWrite];
                uint8_t current_notes = ecg_note_flags[nv_ecg_buffer_select_toWrite][ecg_buffer_index_toWrite];
                __ecg_sample_to_csv(current_sample, current_notes);
            }

            // clear these note files
            memset(ecg_note_flags[nv_ecg_buffer_select_toWrite], 0, ECG_BUFFER_LENGTH);

            // Check the file size and close the file.
            fseek(ecg_data_file, 0L, SEEK_END);
            ecg_data_file_size_b = ftell(ecg_data_file);
            fclose(ecg_data_file);

            // If the file size limit has been reached, start a new file.
            if ((ecg_data_file_size_b >= (long)(ECG_MAX_FILE_SIZE_MB) * 1024L * 1024L || ecg_data_file_size_b < 0) && !g_stopAcquisition)
                init_ecg_data_file(0);

            // CETI_LOG("Wrote %d entries in %lld us", ECG_BUFFER_LENGTH, get_global_time_us() - start_time_us);
        }

        // Advance to the next buffer.
        nv_ecg_buffer_select_toWrite++;
        ecg_buffer_select_toWrite = nv_ecg_buffer_select_toWrite % ECG_NUM_BUFFERS;
    }

    if (!g_stopLogging) {
        ecg_data_file = fopen(ecg_data_filepath, "at");
        if (NULL != ecg_data_file) {
            // flush any complete buffers
            int nv_ecg_buffer_select_toWrite = ecg_buffer_select_toWrite;
            while (shm_ecg->page != nv_ecg_buffer_select_toWrite) {
                // Write the buffer data to the file.
                for (int ecg_buffer_index_toWrite = 0; ecg_buffer_index_toWrite < ECG_BUFFER_LENGTH; ecg_buffer_index_toWrite++) {
                    CetiEcgSample *current_sample = &shm_ecg->data[nv_ecg_buffer_select_toWrite][ecg_buffer_index_toWrite];
                    uint8_t current_notes = ecg_note_flags[nv_ecg_buffer_select_toWrite][ecg_buffer_index_toWrite];
                    __ecg_sample_to_csv(current_sample, current_notes);
                }
                nv_ecg_buffer_select_toWrite = (nv_ecg_buffer_select_toWrite + 1) % ECG_NUM_BUFFERS;
                ecg_buffer_select_toWrite = nv_ecg_buffer_select_toWrite;
            }

            // flush final imcomplete buffers
            for (int ecg_buffer_index_toWrite = 0; ecg_buffer_index_toWrite < shm_ecg->sample; ecg_buffer_index_toWrite++) {
                CetiEcgSample *current_sample = &shm_ecg->data[nv_ecg_buffer_select_toWrite][ecg_buffer_index_toWrite];
                uint8_t current_notes = ecg_note_flags[nv_ecg_buffer_select_toWrite][ecg_buffer_index_toWrite];
                __ecg_sample_to_csv(current_sample, current_notes);
            }

            // close the file.
            fclose(ecg_data_file);
        }
    }

    // Clean up.
    g_ecg_thread_writeData_is_running = 0;
    CETI_LOG("Done!");
    return NULL;
}
