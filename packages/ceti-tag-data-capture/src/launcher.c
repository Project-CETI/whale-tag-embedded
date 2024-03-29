//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "launcher.h"

//-----------------------------------------------------------------------------
// Initialize global variables
//-----------------------------------------------------------------------------
int g_exit = 0;
int g_stopAcquisition = 0;
char g_process_path[256] = "/opt/ceti-tag-data-capture/bin";

MAX17320_HandleTypeDef bms;

void sig_handler(int signum) {
  g_exit = 1;
}

//-----------------------------------------------------------------------------
// Main loop.
//-----------------------------------------------------------------------------
int main(void) {
  // Define callbacks for handling signals.
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  // Get process location
  int bytes = readlink("/proc/self/exe", g_process_path, sizeof(g_process_path) - 1);
  while(bytes > 0) {
    if(g_process_path[bytes - 1] == '/'){
      g_process_path[bytes] = '\0';
      break;
    }
    bytes--;
  }

  // Tag-wide initialization.
  init_logging();

  // Initialize components.
  CETI_LOG("-------------------------------------------------");
  CETI_LOG("Starting initialization");
  init_tag(); // returns 0 if all components succeeded, < 0 if error(s)

  //-----------------------------------------------------------------------------
  // Create threads.
  pthread_t thread_ids[50] = {0};
  int *threads_running[50];
  int num_threads = 0;
  int audio_acquisition_thread_index = -1;
  int audio_write_thread_index = -1;
  CETI_LOG("-------------------------------------------------");
  CETI_LOG("Starting acquisition threads");
// RTC
#if ENABLE_RTC
  pthread_create(&thread_ids[num_threads], NULL, &rtc_thread, NULL);
  threads_running[num_threads] = &g_rtc_thread_is_running;
  num_threads++;
#endif
  // Handle user commands.
  pthread_create(&thread_ids[num_threads], NULL, &command_thread, NULL);
  threads_running[num_threads] = &g_command_thread_is_running;
  num_threads++;
  // Run the state machine.
  pthread_create(&thread_ids[num_threads], NULL, &stateMachine_thread, NULL);
  threads_running[num_threads] = &g_stateMachine_thread_is_running;
  num_threads++;
// IMU
#if ENABLE_IMU
  pthread_create(&thread_ids[num_threads], NULL, &imu_thread, NULL);
  threads_running[num_threads] = &g_imu_thread_is_running;
  num_threads++;
#endif
// Ambient light
#if ENABLE_LIGHT_SENSOR
  pthread_create(&thread_ids[num_threads], NULL, &light_thread, NULL);
  threads_running[num_threads] = &g_light_thread_is_running;
  num_threads++;
#endif
// Water pressure and temperature
#if ENABLE_PRESSURETEMPERATURE_SENSOR
  pthread_create(&thread_ids[num_threads], NULL, &pressureTemperature_thread, NULL);
  threads_running[num_threads] = &g_pressureTemperature_thread_is_running;
  num_threads++;
#endif
// Board temperature
#if ENABLE_BOARDTEMPERATURE_SENSOR
  pthread_create(&thread_ids[num_threads], NULL, &boardTemperature_thread, NULL);
  threads_running[num_threads] = &g_boardTemperature_thread_is_running;
  num_threads++;
#endif
// Battery status monitor
#if ENABLE_BATTERY_GAUGE
  pthread_create(&thread_ids[num_threads], NULL, &battery_thread, NULL);
  threads_running[num_threads] = &g_battery_thread_is_running;
  num_threads++;
#endif
// Recovery board (GPS).
#if ENABLE_RECOVERY
  pthread_create(&thread_ids[num_threads], NULL, &recovery_thread, NULL);
  threads_running[num_threads] = &g_recovery_thread_is_running;
  num_threads++;
#endif
// ECG
#if ENABLE_ECG
  pthread_create(&thread_ids[num_threads], NULL, &ecg_thread_getData, NULL);
  threads_running[num_threads] = &g_ecg_thread_getData_is_running;
  num_threads++;
  pthread_create(&thread_ids[num_threads], NULL, &ecg_thread_writeData, NULL);
  threads_running[num_threads] = &g_ecg_thread_writeData_is_running;
  num_threads++;
#endif
// System resource monitor
#if ENABLE_SYSTEMMONITOR
  pthread_create(&thread_ids[num_threads], NULL, &systemMonitor_thread, NULL);
  threads_running[num_threads] = &g_systemMonitor_thread_is_running;
  num_threads++;
#endif
// GoPros
#if ENABLE_GOPROS
  pthread_create(&thread_ids[num_threads], NULL, &goPros_thread, NULL);
  threads_running[num_threads] = &g_goPros_thread_is_running;
  num_threads++;
#endif
// Audio
#if ENABLE_AUDIO
  usleep(1000000); // wait to make sure all other threads are on their assigned CPUs (maybe not needed?)
  pthread_create(&thread_ids[num_threads], NULL, &audio_thread_spi, NULL);
  threads_running[num_threads] = &g_audio_thread_spi_is_running;
  audio_acquisition_thread_index = num_threads;
  num_threads++;
#if ENABLE_AUDIO_FLAC
  pthread_create(&thread_ids[num_threads], NULL, &audio_thread_writeFlac, NULL);
  threads_running[num_threads] = &g_audio_thread_writeData_is_running;
  audio_write_thread_index = num_threads;
  num_threads++;
#else
  // dump raw audio files
  pthread_create(&thread_ids[num_threads], NULL, &audio_thread_writeRaw, NULL);
  threads_running[num_threads] = &g_audio_thread_writeData_is_running;
  audio_write_thread_index = num_threads;
  num_threads++;
#endif
#endif

  usleep(100000);
  CETI_LOG("Created %d threads", num_threads);

  //-----------------------------------------------------------------------------
  // Run the application!
  CETI_LOG("-------------------------------------------------");
  CETI_LOG("Data acquisition is running!");
  CETI_LOG("-------------------------------------------------");
  while (!g_exit) {
    // Let threads do their work.
    usleep(100000);

// Check if the audio needs to be restarted after an overflow.
#if ENABLE_AUDIO
    if (g_audio_overflow_detected && (g_audio_thread_spi_is_running && g_audio_thread_writeData_is_running)) {
      // Wait for the threads to stop.
      while (g_audio_thread_spi_is_running || g_audio_thread_writeData_is_running)
        usleep(100000);
      // Restart the threads.
      pthread_create(&thread_ids[audio_acquisition_thread_index], NULL, &audio_thread_spi, NULL);
      threads_running[audio_acquisition_thread_index] = &g_audio_thread_spi_is_running;
#if ENABLE_AUDIO_FLAC
      pthread_create(&thread_ids[audio_write_thread_index], NULL, &audio_thread_writeFlac, NULL);
      threads_running[audio_write_thread_index] = &g_audio_thread_writeData_is_running;
#else
      pthread_create(&thread_ids[audio_write_thread_index], NULL, &audio_thread_writeRaw, NULL);
      threads_running[audio_write_thread_index] = &g_audio_thread_writeData_is_running;
#endif
      usleep(100000);
    }
#endif

    // Check if the data partition is full.
    if (!g_stopAcquisition && get_dataPartition_free_kb() < MIN_DATA_PARTITION_FREE_KB) {
      CETI_LOG("*** DATA PARTITION IS FULL. Stopping all threads that acquire data.");
      g_stopAcquisition = 1;
    }
  }
  g_stopAcquisition = 1;
  CETI_LOG("-------------------------------------------------");
  CETI_LOG("Data acquisition completed. Waiting for threads to stop.");

  //-----------------------------------------------------------------------------
  // Give threads time to notice the g_exit flag and shut themselves down.
  int num_threads_running = num_threads;
  int threads_timeout_reached = 0;
#if ENABLE_GOPROS
  int64_t wait_for_threads_timeout_us = 20000000 * NUM_GOPROS;
#else
  int64_t wait_for_threads_timeout_us = 30000000;
#endif
  int64_t wait_for_threads_startTime_us = get_global_time_us();
  while (num_threads_running > 0 && !threads_timeout_reached) {
    usleep(100000);
    num_threads_running = 0;
    for (int thread_index = 0; thread_index < num_threads; thread_index++)
      num_threads_running += *threads_running[thread_index];
    threads_timeout_reached = get_global_time_us() - wait_for_threads_startTime_us > wait_for_threads_timeout_us;
  }

  // Forcefully cancel the threads.
  CETI_LOG("%d/%d threads stopped gracefully.", (int)(num_threads - num_threads_running), num_threads);
  CETI_LOG("Canceling threads");
  for (int thread_index = 0; thread_index < num_threads; thread_index++)
    pthread_cancel(thread_ids[thread_index]);

  // Tag-wide cleanup.
  CETI_LOG("Tag-wide cleanup");
  gpioTerminate();

  CETI_LOG("Done!");

// Save logs to the data partition.
#if ENABLE_SYSTEMMONITOR
  usleep(100000);
  force_system_log_rotation();
#endif

  return (0);
}

//-----------------------------------------------------------------------------
// Helper method to initialize the tag.
//-----------------------------------------------------------------------------
int init_tag() {
  int result = 0;

  // Tag-wide initialization.
  if (gpioInitialise() < 0) {
    CETI_ERR("Failed to initialize pigpio");
    return 1;
  } else {
    CETI_LOG("Successfully initialized pigpio");
  }
  result += init_timing() == 0 ? 0 : -1;
  result += init_commands() == 0 ? 0 : -1;
  result += init_stateMachine() == 0 ? 0 : -1;

// Initialize selectively enabled components.
#if ENABLE_FPGA
  result += init_fpga() == 0 ? 0 : -1;
#endif

#if ENABLE_BATTERY_GAUGE
  result += init_battery() == 0 ? 0 : -1;
  // TODO: Evaluate if a device makes sense vs a global struct
  result += max17320_get_remaining_writes(&bms) == 0 ? 0 : -1;
  result += max17320_get_remaining_capacity(&bms) == 0 ? 0 : -1;
#endif

#if ENABLE_BURNWIRE
  result += init_burnwire() == 0 ? 0 : -1;
#endif

#if ENABLE_AUDIO
  result += init_audio() == 0 ? 0 : -1;
#endif

#if ENABLE_LIGHT_SENSOR
  result += init_light() == 0 ? 0 : -1;
#endif

#if ENABLE_IMU
  result += init_imu() == 0 ? 0 : -1;
#endif

#if ENABLE_RECOVERY
  result += recovery_thread_init() == 0 ? 0 : -1;
#endif

#if ENABLE_BOARDTEMPERATURE_SENSOR
  result += init_boardTemperature() == 0 ? 0 : -1;
#endif

#if ENABLE_PRESSURETEMPERATURE_SENSOR
  result += init_pressureTemperature() == 0 ? 0 : -1;
#endif

#if ENABLE_ECG
  result += init_ecg() == 0 ? 0 : -1;
#endif

#if ENABLE_SYSTEMMONITOR
  result += init_systemMonitor() == 0 ? 0 : -1;
#endif

#if ENABLE_GOPROS
  result += init_goPros() == 0 ? 0 : -1;
#endif

  if (result < 0){
    CETI_ERR("Tag initialization failed (at least one component failed to initialize - see previous printouts for more information)");
    return result;
  }

  sync_global_time_init();

  return result;
}
