//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "goPros.h"
#include <stdlib.h>

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

// Global/static variables
int g_goPros_thread_is_running = 0;
int goPros_python_socket;
int goPros_python_connected_socket;
struct sockaddr_in goPros_python_server;
goPro_python_message_struct goPro_python_message;
char goPros_python_buffer[GOPRO_PYTHON_BUFFERSIZE];
struct sockaddr_in goPros_python_client;

static FILE *goPros_data_file = NULL;
static char goPros_data_file_notes[256] = "";
static const char *goPros_data_file_headers[] = {
    "GoPro Index",
    "Command",
    "Message Ack Time [us]"};
static const int num_goPros_data_file_headers = 3;

int init_goPros()
{
  // Set up the user interface pins.
  gpioSetMode(GOPRO_SWITCH_GPIO, PI_INPUT);
  gpioSetMode(GOPRO_LED_GPIO_GREEN, PI_OUTPUT);
  gpioSetMode(GOPRO_LED_GPIO_BLUE, PI_OUTPUT);
  gpioSetMode(GOPRO_LED_GPIO_RED, PI_OUTPUT);
  usleep(100000);
  for(int i = 0; i < 6; i++)
  {
    gpioWrite(GOPRO_LED_GPIO_GREEN, i % 2 == 0 ? PI_ON : PI_OFF);
    gpioWrite(GOPRO_LED_GPIO_BLUE,  i % 2 == 0 ? PI_ON : PI_OFF);
    gpioWrite(GOPRO_LED_GPIO_RED,   i % 2 == 0 ? PI_ON : PI_OFF);
    usleep(250000);
  }

  // Initialize the Python communication (paths are relative to the bin directory where this program runs).
  system("python3 ../src/sensors/goPros_python_interface/goPros_interface.py &");
  usleep(5000000); // wait until python starts
  if(create_goPros_socket(GOPRO_PYTHON_PORT) < 0)
  {
    CETI_LOG("XXX ERROR: Socket creation or bind failed XXX");
    gpioWrite(GOPRO_LED_GPIO_RED, PI_ON);
    return -1;
  }
  CETI_LOG("Successfully created the Python socket");

  // Open an output file to write data.
  if(init_data_file(goPros_data_file, GOPROS_DATA_FILEPATH,
                     goPros_data_file_headers, num_goPros_data_file_headers,
                     goPros_data_file_notes, "init_goPros()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void *goPros_thread(void *paramPtr)
{
  // Get the thread ID, so the system monitor can check its CPU assignment.
  g_goPros_thread_tid = gettid();

  // Set the thread CPU affinity.
  if(GOPROS_CPU >= 0)
  {
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(GOPROS_CPU, &cpuset);
    if(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) == 0)
      CETI_LOG("Successfully set affinity to CPU %d", GOPROS_CPU);
    else
      CETI_LOG("XXX Failed to set affinity to CPU %d", GOPROS_CPU);
  }

  // Initialize state for the Python socket.
  int client_accepted = 0;
  unsigned int goPros_python_client_length = sizeof(goPros_python_client);

  // Accept the Python connection request.
  CETI_LOG("Waiting for Python client to accept connection");
  while(client_accepted == 0 && !g_stopAcquisition)
  {
    goPros_python_connected_socket = accept(goPros_python_socket, (struct sockaddr *)&goPros_python_client, &goPros_python_client_length);
    client_accepted = (goPros_python_connected_socket >= 0);
    if(client_accepted == 0)
      usleep(200000);
  }
  CETI_LOG("Accepted client connection from Python program (%s)", inet_ntoa(goPros_python_client.sin_addr));
  bzero(goPros_python_buffer, GOPRO_PYTHON_BUFFERSIZE);

  // Main loop while application is running.
  CETI_LOG("Starting loop to control GoPros when needed");
  g_goPros_thread_is_running = 1;
  int goPro_thread_polling_period_us = 10000;
  int goPro_command_success = 0;
  int goPros_are_recording = 0;
  int magnet_detected_debounced = 0;
  int prev_magnet_detected_debounced = 0;
  int magnet_detection_count = 0;
  int magnet_detection_count_max = GOPRO_MAGNET_DETECTION_BUFFER_US/goPro_thread_polling_period_us;
  int magnet_detection_count_threshold = GOPRO_MAGNET_DETECTION_THRESHOLD_US/goPro_thread_polling_period_us;
  long led_heartbeat_recording_duration_us = (8*((long long)GOPRO_LED_HEARTBEAT_PERIOD_US)/10);
  long led_heartbeat_stopped_duration_us = (1*((long long)GOPRO_LED_HEARTBEAT_PERIOD_US)/10);
  long long last_python_heartbeat_us = get_global_time_us();
  long long last_led_heartbeat_us = 0;
  long long led_heartbeat_on_us = -1;
  while(!g_stopAcquisition)
  {
    // Periodically send a heartbeat to let Python know the connection is active.
    if(get_global_time_us() - last_python_heartbeat_us >= GOPRO_PYTHON_HEARTBEAT_PERIOD_US)
    {
      goPro_python_message.goPro_index = NUM_GOPROS + 1;
      goPro_python_message.command = GOPRO_COMMAND_HEARTBEAT;
      goPro_python_message.is_ack = 0;
      goPro_python_message.success = 1;
      send_goPros_message(&goPro_python_message, sizeof(goPro_python_message_struct));
      last_python_heartbeat_us = get_global_time_us();
    }
    // Periodically flash a heartbeat on the LEDs.
    if(get_global_time_us() - last_led_heartbeat_us >= GOPRO_LED_HEARTBEAT_PERIOD_US)
    {
      if(led_heartbeat_on_us == -1)
      {
        gpioWrite(GOPRO_LED_GPIO_GREEN, PI_ON);
        led_heartbeat_on_us = get_global_time_us();
      }
      if(get_global_time_us() - led_heartbeat_on_us >= (goPros_are_recording ? led_heartbeat_recording_duration_us : led_heartbeat_stopped_duration_us))
      {
        gpioWrite(GOPRO_LED_GPIO_GREEN,  PI_OFF);
        last_led_heartbeat_us = get_global_time_us() - (goPros_are_recording ? led_heartbeat_recording_duration_us : led_heartbeat_stopped_duration_us);
        led_heartbeat_on_us = -1;
      }
    }

    // Determine whether the magnet is present, debouncing the result to make a stable verdict.
    // The magnet pulls the GPIO low.
    magnet_detection_count += (!gpioRead(GOPRO_SWITCH_GPIO)) ? 1 : -1;
    if(magnet_detection_count < 0)
      magnet_detection_count = 0;
    if(magnet_detection_count > magnet_detection_count_max)
      magnet_detection_count = magnet_detection_count_max;
    magnet_detected_debounced = magnet_detection_count >= magnet_detection_count_threshold;
    // Indicate whether the magnet is detected.
    gpioWrite(GOPRO_LED_GPIO_BLUE, magnet_detected_debounced ? PI_ON : PI_OFF);

    // Toggle the GoPro recording state if the magnet was removed.
    if(!magnet_detected_debounced && prev_magnet_detected_debounced)
    {
      goPro_command_success = 0;
      for(int i = 0; i < 3 && !goPro_command_success && !g_stopAcquisition; i++)
      {
        if(!goPros_are_recording)
        {
          CETI_LOG("Starting %d GoPros", NUM_GOPROS);
          goPro_command_success = send_command_to_all_goPros(GOPRO_COMMAND_START);
        }
        else
        {
          CETI_LOG("Stopping %d GoPros", NUM_GOPROS);
          goPro_command_success = send_command_to_all_goPros(GOPRO_COMMAND_STOP);
        }
        gpioWrite(GOPRO_LED_GPIO_RED, goPro_command_success ? PI_OFF : PI_ON);
      }
      // Toggle the recording state.
      //  Even if some failed, will want to send the opposite command next time.
      goPros_are_recording = !goPros_are_recording;
    }
    // Update the previous magnet detection state.
    prev_magnet_detected_debounced = magnet_detected_debounced;

    // Delay to limit the loop rate.
    usleep(goPro_thread_polling_period_us);
  }

  // Stop the GoPros in case they weren't stopped manually.
  goPro_command_success = 0;
  for(int i = 0; i < 5 && !goPro_command_success; i++)
  {
    CETI_LOG("Exiting thread; stopping %d GoPros", NUM_GOPROS);
    goPro_command_success = send_command_to_all_goPros(GOPRO_COMMAND_STOP);
  }

  // Clean up and exit.
  close_goPros_socket();
  gpioWrite(GOPRO_LED_GPIO_GREEN,  PI_OFF);
  gpioWrite(GOPRO_LED_GPIO_BLUE, PI_OFF);
  gpioWrite(GOPRO_LED_GPIO_RED,    PI_OFF);
  g_goPros_thread_is_running = 0;
  CETI_LOG("Done!");
  return NULL;
}

//-----------------------------------------------------------------------------
// Socket helpers
//-----------------------------------------------------------------------------

int create_goPros_socket()
{
  if((goPros_python_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  bzero((char *)&goPros_python_server, sizeof(goPros_python_server));
  goPros_python_server.sin_family = AF_INET;
  goPros_python_server.sin_addr.s_addr = INADDR_ANY;
  goPros_python_server.sin_port = htons(GOPRO_PYTHON_PORT);
  if(bind(goPros_python_socket, (struct sockaddr *)&goPros_python_server, sizeof(goPros_python_server)) < 0)
    return -1;

  CETI_LOG("Created socket and completed bind");

  listen(goPros_python_socket, 3);
  return goPros_python_socket;
}

void close_goPros_socket()
{
  close(goPros_python_socket);
  return;
}

int send_goPros_message(void *msg, uint32_t msgsize)
{
  if(write(goPros_python_connected_socket, msg, msgsize) < 0)
    return -1;
  return 0;
}

int send_command_to_all_goPros(int goPro_command)
{
  // Open the data file.
  goPros_data_file = NULL;
  do
  {
    goPros_data_file = fopen(GOPROS_DATA_FILEPATH, "at");
    if(goPros_data_file == NULL)
    {
      CETI_LOG("failed to open data output file: %s", GOPROS_DATA_FILEPATH);
      // Sleep a bit before retrying.
      for(int i = 0; i < 10 && !g_stopAcquisition; i++)
        usleep(500000);
    }
  } while(goPros_data_file == NULL && !g_stopAcquisition);

  // Initialize state.
  int received_client_ack = 0;
  int all_commands_succeeded = 1;
  int num_read_from_client;
  long long time_message_sent_us;
  long long time_message_ack_us;
  long long timeout_start_us;
  int rtc_count_message_sent;

  // Send the command to all GoPros via the Python socket.
  for(int goPro_index = 0; goPro_index < NUM_GOPROS; goPro_index++)
  {
    // Send the command.
    goPro_python_message.goPro_index = goPro_index;
    goPro_python_message.command = goPro_command;
    goPro_python_message.is_ack = 0;
    goPro_python_message.success = 1;
    send_goPros_message(&goPro_python_message, sizeof(goPro_python_message_struct));
    time_message_sent_us = get_global_time_us();
    rtc_count_message_sent = getRtcCount();
    // Wait for confirmation.
    received_client_ack = 0;
    timeout_start_us = get_global_time_us();
    time_message_ack_us = 0;
    while(received_client_ack == 0 && get_global_time_us() - timeout_start_us < GOPRO_PYTHON_ACK_TIMEOUT_US)
    {
      num_read_from_client = read(goPros_python_connected_socket, goPros_python_buffer, GOPRO_PYTHON_BUFFERSIZE);
      if(num_read_from_client > 0)
      {
        goPro_python_message = *((goPro_python_message_struct *)goPros_python_buffer);
        if(goPro_python_message.goPro_index == goPro_index && goPro_python_message.command == goPro_command && goPro_python_message.is_ack == 1)
        {
          received_client_ack = 1;
          all_commands_succeeded &= goPro_python_message.success;
          time_message_ack_us = get_global_time_us();
          CETI_LOG("Sent command %d to GoPro %d", goPro_command, goPro_index);
        }
      }
    }
    // Add an entry to the data file.
    fprintf(goPros_data_file, "%lld", time_message_sent_us);
    fprintf(goPros_data_file, ",%d", rtc_count_message_sent);
    // Write any notes, then clear them so they are only written once.
    fprintf(goPros_data_file, ",%s", goPros_data_file_notes);
    strcpy(goPros_data_file_notes, "");
    // Write the command information.
    fprintf(goPros_data_file, ",%d", goPro_index);
    fprintf(goPros_data_file, ",%d", goPro_command);
    fprintf(goPros_data_file, ",%lld", time_message_ack_us);
    // Finish the row of data.
    fprintf(goPros_data_file, "\n");
  }
  // Close the file.
  fclose(goPros_data_file);

  // Return whether all commands succeeded.
  return all_commands_succeeded;
}