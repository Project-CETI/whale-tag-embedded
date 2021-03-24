#!/usr/bin/python3

# Daniel Vogt (dvogt@seas.harvard.edu)
# V0.2, 3/22/2021

# Script that is launched at startup of the tag to record audio and sensor data

# An initialization time is given at the begining (1Hz blinking) to allow the user to stop the programm via the terminal
# Once done, recording starts and the LED blinks 3x quickly.

# Library imports
from gpiozero import LED
import gzip
import os
import qwiic_icm20948
import subprocess
import sys
import threading
import time
import traceback

threads = []


def blink_forever():
    led = LED(14)
    while True:
        led.off()
        time.sleep(5)
        led.on()
        time.sleep(0.2)

# directory where to save the data
# assume the filesystem for data storage is mounted at /data


def get_data_path():
    try:
        with open("/etc/hostname", "r") as f:
            hname = f.read().strip()
    except BaseException:
        return("/home/pi/")

    datapath = os.path.join("/data", hname)
    if (not os.path.isdir(datapath)):
        try:
            os.mkdir(datapath)
        except:
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

    filename = os.path.join(path,"imu_%d.csv.gz" % time.time())
    with gzip.open(filename, "at") as f:
        f.write("timestamp, ax, ay, az\n")
        while imu.dataReady():
            imu.getAgmt()
            f.write(
                "%f, %d, %d, %d\n" %
                (time.time(), imu.axRaw, imu.ayRaw, imu.azRaw))
            f.flush()
            os.fsync(f.fileno())
            time.sleep(1)
        sys.stderr.write("Error reading IMU: data not ready")


def capture_audio(path=""):
    if (not path):
        sys.stderr.write("No datapath specified")
        return(1)
    while True:
        filename = os.path.join(path, "audio_%d.flac" % time.time())
        command = 'arecord -q -t wav -d 300 -f S16_LE -c 6 -r 96000 | flac - -f --endian=little --channels=6 --bps=16 --sign=signed --sample-rate=96000 -s -o ' + filename
        subprocess.run(command, shell=True)
        # Please note that running sync is a blocking call that takes time.
        # Consider calling sync in non-blocking fashion to start recording the
        # next chunk imediately
        subprocess.run("sync")


def main():
    datapath = get_data_path()
    threads.append(threading.Thread(target=capture_imu, args=(datapath,)))
    threads.append(threading.Thread(target=capture_audio, args=(datapath,)))
    for thread in threads:
        thread.start()
    blink_forever()
    for thread in threads:
        thread.join()

    exit(0)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        os.stderr.write(traceback.format_exc())
        for thread in threads:
            thread.join()
        exit(1)
