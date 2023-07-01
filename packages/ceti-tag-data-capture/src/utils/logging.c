//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "logging.h"

#include "../_versioning.h"

void init_logging() {
    openlog("CETI data capture", LOG_PERROR | LOG_CONS, LOG_USER);
    syslog(LOG_DEBUG, "************************************************");
    syslog(LOG_DEBUG, "            CETI Tag Electronics                ");
    syslog(LOG_DEBUG, " -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
    syslog(LOG_DEBUG, "    Version %s\n", CETI_VERSION);
    syslog(LOG_DEBUG, "    Build date: %s, %s\n", __DATE__, __TIME__);
    syslog(LOG_DEBUG, "*************************************************");
}

int init_data_file(FILE* data_file, const char* data_filepath,
                   const char** data_file_headers, int num_data_file_headers,
                   char* data_file_notes, const char* log_tag)
{
  // Open an output file to write data.
  int data_file_exists = (access(data_filepath, F_OK) != -1);
  data_file = fopen(data_filepath, "at");
  if(data_file == NULL)
  {
      CETI_LOG("%s: Failed to open/create an output data file: %s", log_tag, data_filepath);
      return -1;
  }
  // Write headers if the file didn't already exist.
  if(!data_file_exists)
  {
    char header[500] = "";
    strcat(header,  "Timestamp [us]");
    strcat(header, ",RTC Count");
    strcat(header, ",Notes");
    for(int header_index = 0; header_index < num_data_file_headers; header_index++)
    {
      strcat(header, ",");
      strcat(header, data_file_headers[header_index]);
    }
    strcat(header, "\n");
    fprintf(data_file, "%s", header);
    CETI_LOG("%s: Created a new output data file: %s", log_tag, data_filepath);
  }
  else
    CETI_LOG("%s: Using the existing output data file: %s", log_tag, data_filepath);
  // Add notes for the first timestep to indicate that logging was restarted.
  strcat(data_file_notes, "Restarted! | ");
  // Close the file.
  fclose(data_file);
  return 0;
}