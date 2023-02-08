//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "systemMonitor.h"

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_systemMonitor_thread_is_running = 0;
struct sysinfo memInfo;
static long long ram_total = -1;
static long long virtual_memory_total = -1;
// State for computing CPU usage.
static unsigned long long cpu_prev_user[NUM_CPU_ENTRIES], cpu_prev_userNice[NUM_CPU_ENTRIES];
static unsigned long long cpu_prev_system[NUM_CPU_ENTRIES], cpu_prev_idle[NUM_CPU_ENTRIES];
static unsigned long long cpu_prev_ioWait[NUM_CPU_ENTRIES], cpu_prev_irq[NUM_CPU_ENTRIES], cpu_prev_irqSoft[NUM_CPU_ENTRIES];
static double cpu_percents[NUM_CPU_ENTRIES];
static FILE* cpu_proc_stat_file;
// The main process ID of the program.
static int cetiApp_pid = -1;
// Thread IDs, which will be updated by the relevant threads if they are enabled.
int g_systemMonitor_thread_tid = -1;
int g_audio_thread_spi_tid = -1;
int g_audio_thread_writeData_tid = -1;
int g_ecg_thread_tid = -1;
int g_imu_thread_tid = -1;
int g_light_thread_tid = -1;
int g_pressureTemperature_thread_tid = -1;
int g_boardTemperature_thread_tid = -1;
int g_battery_thread_tid = -1;
int g_recovery_thread_tid = -1;
int g_command_thread_tid = -1;
int g_stateMachine_thread_tid = -1;
// Writing data to a log file.
static FILE* systemMonitor_data_file = NULL;
static char systemMonitor_data_file_notes[256] = "";
static const char* systemMonitor_data_file_headers[] = {
  "CPU all [%]", "CPU 0 [%]", "CPU 1 [%]", "CPU 2 [%]", "CPU 3 [%]",
  "Audio SPI CPU", "Audio Write CPU", "ECG CPU", "IMU CPU", "Light CPU", "PressureTemp CPU",
  "BoardTemp CPU", "Bat CPU", "Recovery CPU", "FSM CPU", "Commands CPU", "SysMonitor CPU",
  "RAM Free [B]", "RAM Free [%]",
  "Virtual Memory Used [B]", "Virtual Memory Used [%]",
  };
static const int num_systemMonitor_data_file_headers = 21;

int init_systemMonitor()
{
  // Get initial readings from /proc/stat, so differences can be taken later to compute CPU usage.
  update_cpu_usage();
  // Get total memory available, which does not change.
  virtual_memory_total = get_virtual_memory_total();
  ram_total = get_ram_total();

  CETI_LOG("init_systemMonitor(): Successfully initialized the system monitor thread");

  // Get the process ID of the program.
  cetiApp_pid = getpid();

  // Open an output file to write data.
  if(init_data_file(systemMonitor_data_file, SYSTEMMONITOR_DATA_FILEPATH,
                     systemMonitor_data_file_headers,  num_systemMonitor_data_file_headers,
                     systemMonitor_data_file_notes, "init_systemMonitor()") < 0)
    return -1;
    
  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* systemMonitor_thread(void* paramPtr) {
    // Get the thread ID, so the system monitor can check its CPU assignment.
    g_systemMonitor_thread_tid = gettid();

    // Set the thread CPU affinity.
    if(SYSTEMMONITOR_CPU >= 0)
    {
      pthread_t thread;
      thread = pthread_self();
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(SYSTEMMONITOR_CPU, &cpuset);
      if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
        CETI_LOG("systemMonitor_thread(): Successfully set affinity to CPU %d", SYSTEMMONITOR_CPU);
      else
        CETI_LOG("systemMonitor_thread(): XXX Failed to set affinity to CPU %d", SYSTEMMONITOR_CPU);
    }

    // Main loop while application is running.
    CETI_LOG("systemMonitor_thread(): Starting loop to periodically check system resources");
    long long virtual_memory_used, ram_free;
    long long global_time_us;
    int rtc_count;
    long long polling_sleep_duration_us;
    g_systemMonitor_thread_is_running = 1;
    while (!g_exit) {
      // Acquire timing and system information as close together as possible.
      global_time_us = get_global_time_us();
      rtc_count = getRtcCount();
      virtual_memory_used = get_virtual_memory_used();
      ram_free = get_ram_free();
      update_cpu_usage();
      
      // Write system usage information to the data file.
      systemMonitor_data_file = fopen(SYSTEMMONITOR_DATA_FILEPATH, "at");
      if(systemMonitor_data_file == NULL)
        CETI_LOG("systemMonitor_thread(): failed to open data output file: %s", SYSTEMMONITOR_DATA_FILEPATH);
      else
      {
        // Write timing information.
        fprintf(systemMonitor_data_file, "%lld", global_time_us);
        fprintf(systemMonitor_data_file, ",%d", rtc_count);
        // Write any notes, then clear them so they are only written once.
        fprintf(systemMonitor_data_file, ",%s", systemMonitor_data_file_notes);
        strcpy(systemMonitor_data_file_notes, "");
        // Write the system usage data.
        for(int cpu_entry_index = 0; cpu_entry_index < NUM_CPU_ENTRIES; cpu_entry_index++)
          fprintf(systemMonitor_data_file, ",%0.2f", cpu_percents[cpu_entry_index]);
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_audio_thread_spi_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_audio_thread_writeData_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_ecg_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_imu_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_light_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_pressureTemperature_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_boardTemperature_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_battery_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_recovery_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_stateMachine_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_command_thread_tid));
        fprintf(systemMonitor_data_file, ",%d", get_cpu_id_for_tid(g_systemMonitor_thread_tid));
        fprintf(systemMonitor_data_file, ",%lld", ram_free);
        fprintf(systemMonitor_data_file, ",%0.2f", 100.0*((double)ram_free)/((double)ram_total));
        fprintf(systemMonitor_data_file, ",%lld", virtual_memory_used);
        fprintf(systemMonitor_data_file, ",%0.2f", 100.0*((double)virtual_memory_used)/((double)virtual_memory_total));
        // Finish the row of data and close the file.
        fprintf(systemMonitor_data_file, "\n");
        fclose(systemMonitor_data_file);
      }

      // Delay to implement a desired sampling rate.
      // Take into account the time it took to acquire/save data.
      polling_sleep_duration_us = SYSTEMMONITOR_SAMPLING_PERIOD_US;
      polling_sleep_duration_us -= get_global_time_us() - global_time_us;
      if(polling_sleep_duration_us > 0)
        usleep(polling_sleep_duration_us);
    }
    g_systemMonitor_thread_is_running = 0;
    CETI_LOG("systemMonitor_thread(): Done!");
    return NULL;
}

// cpu_total virtual memory.
//------------------------------------------
long long get_virtual_memory_total()
{
  sysinfo(&memInfo);
  long long totalVirtualMem = memInfo.totalram;
  //Add other values in next statement to avoid int overflow on right hand side...
  totalVirtualMem += memInfo.totalswap;
  totalVirtualMem *= memInfo.mem_unit;
  return totalVirtualMem;
}

// Virtual memory used.
//------------------------------------------
long long get_virtual_memory_used()
{
  long long virtualMemUsed = memInfo.totalram - memInfo.freeram;
  //Add other values in next statement to avoid int overflow on right hand side...
  virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
  virtualMemUsed *= memInfo.mem_unit;
  return virtualMemUsed;
}

long long get_ram_total()
{
  long long totalPhysMem = memInfo.totalram;
  //Multiply in next statement to avoid int overflow on right hand side...
  totalPhysMem *= memInfo.mem_unit;
  return totalPhysMem;
}

// Note that this will not consider buffer/cache, so the usage reported
//  will not match the usage reported by the "free" command.
long long get_ram_used()
{
  long long physMemUsed = memInfo.totalram - memInfo.freeram;
  //Multiply in next statement to avoid int overflow on right hand side...
  physMemUsed *= memInfo.mem_unit;
  return physMemUsed;
}

long long get_ram_free()
{
  long long physMemFree = memInfo.freeram;
  //Multiply in next statement to avoid int overflow on right hand side...
  physMemFree *= memInfo.mem_unit;
  return physMemFree;
}

// CPU usage.
//------------------------------------------
int update_cpu_usage()
{
    unsigned long long cpu_user, cpu_userNice, cpu_system, cpu_idle;
    unsigned long long cpu_ioWait, cpu_irq, cpu_irqSoft;
    unsigned long long cpu_total;

    cpu_proc_stat_file = fopen("/proc/stat", "r");
    char* line = NULL;
    size_t line_len = 0;
    ssize_t num_read;
    char sscanf_format_string[100];
    for(int cpu_entry_index = 0; cpu_entry_index < NUM_CPU_ENTRIES; cpu_entry_index++)
    {
      num_read = getline(&line, &line_len, cpu_proc_stat_file);
      if(num_read >= 0)
      {
        //printf("CPU entry %d:\n", cpu_entry_index);
        //printf("Retrieved line of length %d:\n", num_read);
        //printf("%s\n", line);
        if(cpu_entry_index == 0)
          sprintf(sscanf_format_string, "cpu %%llu %%llu %%llu %%llu %%llu %%llu %%llu");
        else
          sprintf(sscanf_format_string, "cpu%d %%llu %%llu %%llu %%llu %%llu %%llu %%llu",
                  cpu_entry_index-1);
        int res = sscanf(line, sscanf_format_string,
                          &cpu_user, &cpu_userNice, &cpu_system, &cpu_idle,
                          &cpu_ioWait, &cpu_irq, &cpu_irqSoft);
        //printf("Res: %d\n", res);
        //printf("Read cpu_user |%lld|\n", cpu_user );
        //printf("Read cpu_userNice |%lld|\n", cpu_userNice );
        //printf("Read cpu_system |%lld|\n", cpu_system );
        //printf("Read cpu_idle |%lld|\n", cpu_idle );
        //printf("Read cpu_ioWait |%lld|\n", cpu_ioWait );
        //printf("Read cpu_irq |%lld|\n", cpu_irq );
        //printf("Read cpu_irqSoft |%lld|\n", cpu_irqSoft );
        //printf("\n");

        if(res < 7)
          cpu_percents[cpu_entry_index] = -1.0;
        else if(cpu_user < cpu_prev_user[cpu_entry_index] || cpu_userNice < cpu_prev_userNice[cpu_entry_index] ||
            cpu_system < cpu_prev_system[cpu_entry_index] || cpu_idle < cpu_prev_idle[cpu_entry_index] ||
            cpu_ioWait < cpu_prev_ioWait[cpu_entry_index] || cpu_irq < cpu_prev_irq[cpu_entry_index] || cpu_irqSoft < cpu_prev_irqSoft[cpu_entry_index])
        {
          //Overflow detection. Just skip this value.
          cpu_percents[cpu_entry_index] = -1.0;
        }
        else
        {
          cpu_total = (cpu_user - cpu_prev_user[cpu_entry_index]) + (cpu_userNice - cpu_prev_userNice[cpu_entry_index])
                      + (cpu_system - cpu_prev_system[cpu_entry_index]);
          cpu_percents[cpu_entry_index] = cpu_total;
          cpu_total += (cpu_idle - cpu_prev_idle[cpu_entry_index]) + (cpu_ioWait - cpu_prev_ioWait[cpu_entry_index])
                        + (cpu_irq - cpu_prev_irq[cpu_entry_index]) + (cpu_irqSoft - cpu_prev_irqSoft[cpu_entry_index]);
          cpu_percents[cpu_entry_index] /= cpu_total;
          cpu_percents[cpu_entry_index] *= 100;
        }

        cpu_prev_user[cpu_entry_index] = cpu_user;
        cpu_prev_userNice[cpu_entry_index] = cpu_userNice;
        cpu_prev_system[cpu_entry_index] = cpu_system;
        cpu_prev_idle[cpu_entry_index] = cpu_idle;
        cpu_prev_ioWait[cpu_entry_index] = cpu_ioWait;
        cpu_prev_irq[cpu_entry_index] = cpu_irq;
        cpu_prev_irqSoft[cpu_entry_index] = cpu_irqSoft;
      }
      else
      {
        for(int cpu_entry_index = 0; cpu_entry_index < NUM_CPU_ENTRIES; cpu_entry_index++)
          cpu_percents[cpu_entry_index] = -1.0;
        break;
      }
    }
    fclose(cpu_proc_stat_file);

    return 0;
}

// Get the CPU on which a process is currently running.
// NOTE: Previously tried calling sched_getcpu() and getcpu() from within each thread,
//       but neither output matched the result of htop, top, or ps. 
int get_cpu_id_for_tid(int tid)
{
  if(tid < 0 || cetiApp_pid < 0)
    return -1;

  // Run the ps command with the given process ID.
  FILE *ps_command_file;
  char ps_command_string[40];
  sprintf(ps_command_string, "ps --pid %d -o tid=,psr= -L", cetiApp_pid);
  ps_command_file = popen(ps_command_string, "r");
  if(ps_command_file == NULL)
    return -1;
  // Parse the result.
  char* line = NULL;
  size_t line_len = 0;
  int ps_tid = 0;
  int ps_cpu_id = 0;
  while(getline(&line, &line_len, ps_command_file) >= 0)
  {
    int res = sscanf(line, "%d   %d", &ps_tid, &ps_cpu_id);
    if(res != 2)
      return -1;
    if(ps_tid == tid)
      break;
  }
  // Close the command file pointer.
  pclose(ps_command_file);

  return ps_cpu_id;
}

//// Virtual memory used by current process.
////------------------------------------------
//int parseLine(char* line){
//    // This assumes that a digit will be found and the line ends in " Kb".
//    int i = strlen(line);
//    const char* p = line;
//    while (*p <'0' || *p > '9') p++;
//    line[i-3] = '\0';
//    i = atoi(p);
//    return i;
//}
//
//int getValue(){ //Note: this value is in KB!
//    FILE* file = fopen("/proc/self/status", "r");
//    int result = -1;
//    char line[128];
//
//    while (fgets(line, 128, file) != NULL){
//        if (strncmp(line, "VmSize:", 7) == 0){
//            result = parseLine(line);
//            break;
//        }
//    }
//    fclose(file);
//    return result;
//}

//// RAM used by current process.
////------------------------------------------
//int getValue(){ //Note: this value is in KB!
//    FILE* file = fopen("/proc/self/status", "r");
//    int result = -1;
//    char line[128];
//
//    while (fgets(line, 128, file) != NULL){
//        if (strncmp(line, "VmRSS:", 6) == 0){
//            result = parseLine(line);
//            break;
//        }
//    }
//    fclose(file);
//    return result;
//}



