//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "goPros.h"

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

static FILE* goPros_data_file = NULL;
static char goPros_data_file_notes[256] = "";
static const char* goPros_data_file_headers[] = {
  "GoPro Index",
  "Command",
  "Message Ack Time [us]"
  };
static const int num_goPros_data_file_headers = 3;


int init_goPros() {
  // Initialize the Python communication.
  if(create_goPros_socket(GOPRO_PYTHON_PORT) < 0)
  {
    CETI_LOG("init_goPros(): XXX ERROR: Socket creation or bind failed XXX");
    return -1;
  }
  CETI_LOG("init_goPros(): Successfully created the Python socket");

  // Open an output file to write data.
  if(init_data_file(goPros_data_file, GOPROS_DATA_FILEPATH,
                     goPros_data_file_headers,  num_goPros_data_file_headers,
                     goPros_data_file_notes, "init_goPros()") < 0)
    return -1;

  return 0;
}

//-----------------------------------------------------------------------------
// Main thread
//-----------------------------------------------------------------------------
void* goPros_thread(void* paramPtr) {
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
        CETI_LOG("goPros_thread(): Successfully set affinity to CPU %d", GOPROS_CPU);
      else
        CETI_LOG("goPros_thread(): XXX Failed to set affinity to CPU %d", GOPROS_CPU);
    }

    // Initialize state for socket.
    int client_accepted = 0;
    unsigned int goPros_python_client_length = sizeof(goPros_python_client);

    // Accept Python connection request.
    CETI_LOG("goPros_thread(): Waiting for Python client to accept connection");
    while(client_accepted == 0 && !g_exit)
    {
      goPros_python_connected_socket = accept(goPros_python_socket, (struct sockaddr*)&goPros_python_client, &goPros_python_client_length);
      client_accepted = (goPros_python_connected_socket >= 0);
      if(client_accepted == 0)
        usleep(200000);
    }
    CETI_LOG("goPros_thread(): Accepted client connection from Python program (%s)", inet_ntoa(goPros_python_client.sin_addr));
    bzero(goPros_python_buffer, GOPRO_PYTHON_BUFFERSIZE);

    // Start the GoPros!
    if(!g_exit)
    {
      CETI_LOG("goPros_thread(): Starting the GoPros");
      send_command_to_all_goPros(GOPRO_COMMAND_START);
    }

    // Main loop while application is running.
    CETI_LOG("goPros_thread(): Starting loop to wait while data acquisition runs");
    g_goPros_thread_is_running = 1;
    usleep(GOPRO_PYTHON_HEARTBEAT_PERIOD_US);
    while(!g_exit) {
      // Periodically send a heartbeat to let Python know the connection is active.
      goPro_python_message.goPro_index = NUM_GOPROS+1;
      goPro_python_message.command = GOPRO_COMMAND_HEARTBEAT;
      goPro_python_message.is_ack = 0;
      send_goPros_message(&goPro_python_message, sizeof(goPro_python_message_struct));
      usleep(GOPRO_PYTHON_HEARTBEAT_PERIOD_US);
    }

    // Stop the GoPros
    CETI_LOG("goPros_thread(): Stopping the GoPros");
    send_command_to_all_goPros(GOPRO_COMMAND_STOP);

    // Clean up and exit.
    close_goPros_socket();
    g_goPros_thread_is_running = 0;
    CETI_LOG("goPros_thread(): Done!");
    return NULL;
}

//-----------------------------------------------------------------------------
// Socket helpers
//-----------------------------------------------------------------------------

int create_goPros_socket()
{
    if((goPros_python_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    bzero((char *) &goPros_python_server, sizeof(goPros_python_server));
    goPros_python_server.sin_family = AF_INET;
    goPros_python_server.sin_addr.s_addr = INADDR_ANY;
    goPros_python_server.sin_port = htons(GOPRO_PYTHON_PORT);
    if(bind(goPros_python_socket, (struct sockaddr *)&goPros_python_server , sizeof(goPros_python_server)) < 0)
        return -1;

    CETI_LOG("create_goPros_socket(): Created socket and completed bind");

    listen(goPros_python_socket, 3);
    return goPros_python_socket;
}

void close_goPros_socket()
{
    close(goPros_python_socket);
    return;
}

int send_goPros_message(void* msg, uint32_t msgsize)
{
    if(write(goPros_python_connected_socket, msg, msgsize) < 0)
        return -1;
    return 0;
}

void send_command_to_all_goPros(int goPro_command)
{
  // Open the data file.
  goPros_data_file = NULL;
  do
  {
    goPros_data_file = fopen(GOPROS_DATA_FILEPATH, "at");
    if(goPros_data_file == NULL)
    {
      CETI_LOG("send_command_to_all_goPros(): failed to open data output file: %s", GOPROS_DATA_FILEPATH);
      // Sleep a bit before retrying.
      for(int i = 0; i < 10 && !g_exit; i++)
        usleep(500000);
    }
  } while(goPros_data_file == NULL && !g_exit);

  // Initialize state.
  int received_client_ack = 0;
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
    send_goPros_message(&goPro_python_message, sizeof(goPro_python_message_struct));
    time_message_sent_us = get_global_time_us();
    rtc_count_message_sent = getRtcCount();
    // Wait for confirmation.
    received_client_ack = 0;
    timeout_start_us = get_global_time_us();
    while(received_client_ack == 0 && get_global_time_us() - timeout_start_us < 1000000)
    {
      num_read_from_client = read(goPros_python_connected_socket, goPros_python_buffer, GOPRO_PYTHON_BUFFERSIZE);
      if(num_read_from_client > 0)
      {
        goPro_python_message = *((goPro_python_message_struct*) goPros_python_buffer);
        if(goPro_python_message.goPro_index == goPro_index
            && goPro_python_message.command == goPro_command
            && goPro_python_message.is_ack == 1)
        {
          received_client_ack = 1;
          time_message_ack_us = get_global_time_us();
          CETI_LOG("send_command_to_all_goPros(): Sent command %d to GoPro %d", goPro_command, goPro_index);
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
}