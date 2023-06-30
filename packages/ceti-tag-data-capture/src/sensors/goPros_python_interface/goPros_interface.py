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

# Define the C-like struct that will be sent from the C program.
class GoPro_C_Message(Structure):
  _fields_ = [
    ('goPro_index', c_uint32),
    ('command', c_uint32),
    ('is_ack', c_uint32),
    ('success', c_uint32),
  ]

# Define the commands.
goPro_commands = {
  'start': 1,
  'stop': 2,
  'heartbeat': 3,
}

# Define the GoPros.
goPro_ids = [6059, 7192]
num_goPros = len(goPro_ids) # note that this can be greater than the number in the C program (they will simply not be started)

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
    
    # Get the ID of the GoPro to control.
    goPro_id = goPro_ids[goPro_index]

    # Check if the command is redundant.
    if command == goPro_commands['start'] and goPro_status[goPro_index] == 1:
        logger.info("GoPro %d (ID %d) is already running; ignoring start command" % (goPro_index, goPro_id))
        print('GoPro %d (ID %d) is already running; ignoring start command' % (goPro_index, goPro_id))
        return True
    if command == goPro_commands['stop'] and goPro_status[goPro_index] == 0:
        logger.info("GoPro %d (ID %d) is already stopped; ignoring stop command" % (goPro_index, goPro_id))
        print('GoPro %d (ID %d) is already stopped; ignoring stop command' % (goPro_index, goPro_id))
        return True
    
    # Create a client for the GoPro.
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
    
    # Connect to the GoPro
    try:
        client = await asyncio.wait_for(connect_ble(notification_handler, goPro_id),
                                        timeout=20)
        event.clear()
    except (asyncio.TimeoutError, # The device was probably not found in the Bluetooth scan
            RuntimeError):        # Probably exceeded the 10 retry attempts to connect
        event.clear()
        logger.info("Error or timeout connecting to GoPro %d (ID %d)" % (goPro_index, goPro_id))
        print("Error or timeout connecting to GoPro %d (ID %d)" % (goPro_index, goPro_id))
        return False
    
    # Start the GoPro.
    if command == goPro_commands['start']: # Write to command request BleUUID to turn the shutter on
        logger.info("Setting the shutter on for GoPro %d (ID %d)" % (goPro_index, goPro_id))
        print('Starting GoPro %d (ID %d)' % (goPro_index, goPro_id))
        await client.write_gatt_char(COMMAND_REQ_UUID, bytearray([3, 1, 1, 1]), response=True)
        goPro_status[goPro_index] = 1
    # Stop the GoPro.
    elif command == goPro_commands['stop']: # Write to command request BleUUID to turn the shutter off
        logger.info("Setting the shutter off for GoPro %d (ID %d)" % (goPro_index, goPro_id))
        print('Stopping GoPro %d (ID %d)' % (goPro_index, goPro_id))
        await client.write_gatt_char(COMMAND_REQ_UUID, bytearray([3, 1, 1, 0]), response=True)
        goPro_status[goPro_index] = 0
        
    await event.wait()  # Wait to receive the notification response
    await client.disconnect()
    return True
    
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
                        success = await control_goPro(goPro_index, command)
                        c_message = GoPro_C_Message(goPro_index, command, 1, 1 if success else 0)
                        c_socket.sendall(c_message)
                    # Update alive time.
                    last_message_time_s = time.time()
        except socket.error as socket_error:
            print('Exception on socket: %s' % str(socket_error))

    # The C program stopped sending a heartbeat, so close the connection and restart.
    print('No heartbeat! Exiting the program.')
    if is_connected:
        print('Closing the socket')
        c_socket.close()
    del c_socket

    # Ensure that the GoPros are stopped.
    if sum(goPro_status) > 0:
        command = goPro_commands['stop']
        for x in range(num_goPros):
            if goPro_status[x]==1:
                print('Stopping GoPro', x)
                await control_goPro(x, command)
                goPro_status[x] = 0
    
    print('Program completed.')

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except Exception as e:
        logger.error(e)
        sys.exit(-1)
    else:
        sys.exit(0)