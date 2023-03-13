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
import asyncio
from typing import Optional
from binascii import hexlify

from bleak import BleakClient
from tutorial_modules import GOPRO_BASE_UUID, connect_ble, logger
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
num_goPros = 2
goPro_commands = {
  'start': 1,
  'stop': 2,
  'heartbeat': 3,
}

# Socket configuration
goPro_c_port = 2300
connection_heartbeat_period_s = 5
socket_receive_timeout_s = 5
goPro_status = [0] * num_goPros # used to check whether the goPro is now on or off
####################################################################
# Create the GoPro interface
####################################################################
async def control_goPro(goPro_index, command):
    # Synchronization event to wait until notification response is received
    event = asyncio.Event()
    # UUIDs to write to and receive responses from
    COMMAND_REQ_UUID = GOPRO_BASE_UUID.format("0072")
    COMMAND_RSP_UUID = GOPRO_BASE_UUID.format("0073")
    response_uuid = COMMAND_RSP_UUID
    #logger.info(goPro_index)
    
    if(goPro_index==0):
        goPro_Name = 6059
    elif(goPro_index==1):
        goPro_Name = 7192
    elif(goPro_index==2): # We have to change it to Camera name as we get them.
        goPro_Name = 7777    
    elif(goPro_index==3): # We have to change it to Camera name as we get them.
        goPro_Name = 7777
    else:
        goPro_Name = 7777
            
    client: BleakClient

    def notification_handler(handle: int, data: bytes) -> None:
        logger.info(f'Received response at {handle=}: {hexlify(data, ":")!r}')

        # If this is the correct handle and the status is success, the command was a success
        if client.services.characteristics[handle].uuid == response_uuid and data[2] == 0x00:
            logger.info("Command sent successfully")
        # Anything else is unexpected. This shouldn't happen
        else:
            logger.error("Unexpected response")

        # Notify the writer
        event.set()

    client = await connect_ble(notification_handler, goPro_Name)
    event.clear()
    if (command == goPro_commands['start'] and goPro_status[goPro_index]==0):        # Write to command request BleUUID to turn the shutter on
        logger.info("Setting the shutter on")
        print('Starting GoPro %d' % goPro_Name)
        await client.write_gatt_char(COMMAND_REQ_UUID, bytearray([3, 1, 1, 1]), response=True)
        goPro_status[goPro_index] = 1
    elif (command == goPro_commands['stop'] and goPro_status[goPro_index]==1):        # Write to command request BleUUID to turn the shutter off
        logger.info("Setting the shutter off")
        print('Stopping GoPro %d' % goPro_Name)
        await client.write_gatt_char(COMMAND_REQ_UUID, bytearray([3, 1, 1, 0]), response=True)
        goPro_status[goPro_index] = 0
    await event.wait()  # Wait to receive the notification response
    await client.disconnect()
    
####################################################################
# Main loop to listen to and process commands.
####################################################################
async def main() -> None:
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
        #return
    
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

                    # Send an acknowledgment if it is not a heartbeat.
                    if command != goPro_commands['heartbeat']:
                        await control_goPro(goPro_index, command)
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

    if(sum(goPro_status)): # If c code stops, it automatically stops the goPro cameras
        command = 2
        for x in range(num_goPros):
            if(goPro_status[x]==1):
                await control_goPro(x, command)
                goPro_status[x] = 0

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except Exception as e:
        logger.error(e)
        sys.exit(-1)
    else:
        sys.exit(0)