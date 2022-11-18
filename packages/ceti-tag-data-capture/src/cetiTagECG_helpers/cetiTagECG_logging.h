//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Created by Joseph DelPreto, within framework by Cummings Electronics Labs, August 2022
// Developed for Harvard University Wood Lab
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagECG_logging.h
// Description: Helpers for saving ECG data
//-----------------------------------------------------------------------------

#ifndef CETI_ECG_LOGGING_H
#define CETI_ECG_LOGGING_H

#include "../cetiTagLogging.h" // for CETI_LOG()
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

// Initialization and clean-up.
int ecg_log_setup();
void ecg_log_cleanup();

// Adding new entries to the log.
void ecg_log_add_entry(long long sample_index, long ecg_reading, int lod_reading);

// Internal state.
extern char ecg_log_filepath[100];
extern FILE *ecg_log_fout;
extern struct timeval ecg_log_entry_time_timeval;
extern long long ecg_log_entry_time_us;

#endif // CETI_ECG_LOGGING_H




