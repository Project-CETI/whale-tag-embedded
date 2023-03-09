#-----------------------------------------------------------------------------
# Project:      CETI Tag Electronics
# Version:      Refer to _versioning.h
# Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
# Contributors: Byungchul Kim, Joseph DelPreto [TODO: Add other contributors here]
#-----------------------------------------------------------------------------

import socket
import select
import sys
import time
from ctypes import *

####################################################################
# Define the C interface, which should match the definitions in sensors/goPros.h
####################################################################

# Define the C-like struct.
class GoPro_C_Message(Structure):
  _fields_ = [
    ('goPro_index', c_uint32),
    ('command', c_uint32),
    ('is_ack', c_uint32),
  ]

# Define the commands.
num_goPros = 4
goPro_commands = {
  'start': 1,
  'stop': 2,
  'heartbeat': 3,
}

# Socket configuration
goPro_c_port = 2300
connection_heartbeat_period_s = 5
socket_receive_timeout_s = 5

####################################################################
# Create the GoPro interface
####################################################################
def control_goPro(goPro_index, command):
  if command == goPro_commands['start']:
    print('Starting GoPro %d' % goPro_index)
  elif command == goPro_commands['stop']:
    print('Stopping GoPro %d' % goPro_index)

####################################################################
# Main loop to listen to and process commands.
####################################################################
def main():
  while True:
    # Create the socket.
    print('Creating the socket')
    c_server_address = ('localhost', goPro_c_port)
    c_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    c_socket.setblocking(0)
    
    # Repeatedly try to connect to the C server.
    print('Waiting for connection to be accepted')
    is_connected = False
    waiting_start_s = time.time()
    while not is_connected and time.time() - waiting_start_s < 10:
      try:
        c_socket.connect(c_server_address)
        is_connected = True
        print('Connected to %s' % repr(c_server_address))
      except AttributeError as ae:
        pass
        #print('Error creating the socket: %s' % str(ae))
      except socket.error as se:
        pass
        #print('Exception on socket: %s' % str(se))
      if not is_connected:
        time.sleep(0.1)
    if not is_connected:
      c_socket.close()
      time.sleep(1)
      continue
    
    # Continuously wait for and process commands from the C program.
    last_message_time_s = time.time()
    while time.time() - last_message_time_s < 2*connection_heartbeat_period_s:
      # print('Time since last message: %0.2f' % (time.time() - last_message_time_s))
      try:
        data_is_available = select.select([c_socket], [], [], socket_receive_timeout_s)[0]
        if data_is_available:
          buff = c_socket.recv(sizeof(GoPro_C_Message))
          if len(buff) > 0:
            # Parse the message.
            c_message = GoPro_C_Message.from_buffer_copy(buff)
            goPro_index = c_message.goPro_index
            command = c_message.command
            print('Received command %d for goPro index %d' % (command, goPro_index))
            # Process the command.
            control_goPro(goPro_index, command)
            # Send an acknowledgment if it is not a heartbeat.
            if command != goPro_commands['heartbeat']:
              c_message = GoPro_C_Message(goPro_index, command, 1)
              c_socket.sendall(c_message)
            # Update alive time.
            last_message_time_s = time.time()
      except socket.error as socket_error:
        print('Exception on socket: %s' % str(socket_error))
    
    # The C program stopped sending a heartbeat, so close the connection and restart.
    print('No heartbeat! Closing the socket')
    c_socket.close()
    del c_socket


if __name__ == '__main__':
  main()