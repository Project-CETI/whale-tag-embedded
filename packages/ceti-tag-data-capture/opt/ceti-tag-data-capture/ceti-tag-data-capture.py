#!/usr/bin/python3

# Daniel Vogt (dvogt@seas.harvard.edu)
# V0.2, 3/22/2021

# Script that is launched at startup of the tag to record audio and sensor data

# An initialization time is given at the begining (1Hz blinking) to allow
# the user to stop the programm via the terminal.
# Once done, recording starts and the LED blinks 3x quickly.

# Library imports
from gpiozero import LED
import gzip
import multiprocessing
import os
import qwiic_icm20948
import socket
import subprocess
import sys
import time
import traceback


def blink_forever():
    led = LED(14)
    while True:
        led.off()
        time.sleep(5)
        led.on()
        time.sleep(0.2)


def get_data_path():
    # directory where to save the data
    # assume the filesystem for data storage is mounted at /data
    try:
        hname = socket.gethostname().strip()
    except BaseException:
        return("/home/pi/")

    datapath = os.path.join("/data", hname)
    if (not os.path.isdir(datapath)):
        try:
            os.mkdir(datapath)
        except BaseException:
            return("/home/pi/")
    return datapath


def capture_imu(path=""):
    imu = qwiic_icm20948.QwiicIcm20948()
    imu.begin()
    if (not imu.connected):
        sys.stderr.write("Failed to connect to IMU")
        return(1)

    if (not path):
        sys.stderr.write("No datapath specified")
        return(1)

    filename = os.path.join(path, "imu_%d.csv.gz" % time.time())
    with gzip.open(filename, "at") as f:
        f.write("timestamp, ax, ay, az\n")
        while imu.dataReady():
            imu.getAgmt()
            f.write(
                "%f, %d, %d, %d\n" %
                (time.time(), imu.axRaw, imu.ayRaw, imu.azRaw))
            f.flush()
            os.fsync(f.fileno())
            # Capture IMU data at 1Hz
            # Note that flush() and fsync() calls can take time too
            # Consider timing those calls and substract time taken
            # from the 1 second sleep that is needed for 1Hz freq.
            time.sleep(1)
        sys.stderr.write("Error reading IMU: data not ready")


def capture_audio(path=""):
    if (not path):
        sys.stderr.write("No datapath specified")
        return(1)
    while True:
        filename = os.path.join(path, "audio_%d.flac" % time.time())
        command_injector_zero = 'arecord --device=hw:0,0 -q -t wav -d 300 -f S16_LE -c 2 -r 96000 | flac - -f -s -o ' + filename
        # command_octo = 'arecord -q -t wav -d 300 -f S16_LE -c 6 -r 96000 | flac - -f -s -o ' + filename
        # Make sure to pass the correct command based on your current hardware
        subprocess.run(command_injector_zero, shell=True)


def main():
    Processes = []
    try:
        datapath = get_data_path()
        sys.stdout.write("Starting data recording to " + datapath)
        Processes.append(
            multiprocessing.Process(
                target=capture_imu, args=(
                    datapath,)))
        Processes.append(
            multiprocessing.Process(
                target=capture_audio, args=(
                    datapath,)))
        for process in Processes:
            process.start()
        blink_forever()
    except Exception:
        os.stderr.write(traceback.format_exc())
        for process in Processes:
            process.terminate()
        exit(1)
    exit(0)


if __name__ == "__main__":
    main()
