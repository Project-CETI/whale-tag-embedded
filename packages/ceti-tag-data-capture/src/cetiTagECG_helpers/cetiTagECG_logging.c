//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG_logging.c
// Description: Helpers for saving ECG data
//-----------------------------------------------------------------------------

#include "cetiTagECG_logging.h"

// Variables declared as extern in the header file.
char ecg_log_filepath[100];
FILE *ecg_log_fout;
struct timeval ecg_log_entry_time_timeval;
long long ecg_log_entry_time_us;

// ========================================================
// =============== INITIALIZATION / CLEANUP ===============
// ========================================================

int ecg_log_setup()
{
  // Create a filename for the log.
  struct timeval tv;
  time_t t;
  struct tm *info;
  char buffer[64];
  gettimeofday(&tv, NULL);
  t = tv.tv_sec;
  info = localtime(&t);
  strftime(buffer, sizeof buffer, "%Y-%m-%d_%H-%M-%S", info);
  sprintf(ecg_log_filepath, "../data/%s_ecg.csv", buffer);

  // Open the file.
  ecg_log_fout = fopen(ecg_log_filepath, "w");
  if(ecg_log_fout == NULL)
  {
      CETI_LOG("ecg_log_setup(): failed to open the ECG log file: %s\n", ecg_log_filepath);
      return 0;
  }
  CETI_LOG("ecg_log_setup(): opened an ECG log file: %s\n", ecg_log_filepath);

  // Write headers to the file.
  char headers[] = "Sample Index,Timestamp [us],ECG,Leads-Off";
  fprintf(ecg_log_fout, "%s\n", headers);

  return 1;
}

void ecg_log_cleanup()
{
  // Close the log file.
  CETI_LOG("ecg_log_cleanup(): Closing the ECG log file.\n");
  fclose(ecg_log_fout);
}

// ========================================================
// ===================== LOGGING DATA =====================
// ========================================================

void ecg_log_add_entry(long long sample_index, long ecg_reading, int lod_reading)
{
  // Timestamp the entry using microseconds.
  gettimeofday(&ecg_log_entry_time_timeval, NULL);
  ecg_log_entry_time_us = (long long)(ecg_log_entry_time_timeval.tv_sec * 1000000LL)
                            + (long long)(ecg_log_entry_time_timeval.tv_usec);

  // Write the entry.
  fprintf(ecg_log_fout, "%lld,", sample_index);
  fprintf(ecg_log_fout, "%lld,", ecg_log_entry_time_us);
  fprintf(ecg_log_fout, "%ld,", ecg_reading);
  fprintf(ecg_log_fout, "%d,", lod_reading);
  fprintf(ecg_log_fout, "\n");
  fflush(ecg_log_fout); // may be redundant with writing a newline above
}





