# Daniel Vogt (dvogt@seas.harvard.edu)
# V0.1, 3/16/2021

# Script that is launched at startup of the tag to record audio and sensor data

# An initialization time is given at the begining (1Hz blinking) to allow the user to stop the programm via the terminal
# Once done, recording starts and the LED blinks 3x quickly.

# TO DO
# - fix bug audio file numbers (sorting list of wav files)
# - adjust exact timing in main loop
# - define timestamp for CSV file data
# - 

# Library imports
from gpiozero import LED
import time
from time import sleep
import datetime
import os
import subprocess
import glob
import qwiic_icm20948

############################
# VARIABLES
############################

# time before start of recording
InitializationTime = 10 # seconds

# The programm stops running if it reaches that time
MaxRunningTime = 35 # seconds

# Status LED pin
led = LED(14) 

# get timestamp
StartTimestamp = datetime.datetime.now().strftime('%m-%d-%Y_%H.%M.%S')

# sampling rate variables
ts_sensors = 1 # second

# measurement variable for battery level
Meas_Batt = 0 # V

# Battery threshold to stop recording
CriticalBattLevel = 3.2 # V

# Define folders
WaveFolder = "/home/pi/wav/"
FlacFolder = "/home/pi/flac/"
CsvFolder = "/home/pi/csv/"

# Variable to enable lossless compression
ConvertToFlac = True # True or False

# process variable that converts wav to flac, creating dummy
p_compress= subprocess.Popen(["echo"])

# IMU object
IMU = qwiic_icm20948.QwiicIcm20948()



############################
# FUNCTIONS
############################

# blink function
def Blink_nTimes(nTimes):
  for i in range(0,nTimes):
    print('LED on')
    led.on()
    sleep(0.1)
    print('LED off')
    led.off()
    sleep(0.1)


############################
# SETUP
############################

# initiate IMU
IMU.begin()
if IMU.connected == False:
    print("The Qwiic ICM20948 device isn't connected to the system. Please check your connection", \
        file=sys.stderr)

# CSV file
CSV_file = open(CsvFolder+StartTimestamp+'_data.csv', "w")
CSV_file.write("timestamp, ax,ay,az \n")
CSV_file.close()

############################
# PROGRAMM START
############################

print('StartupScript.py started.')    

# initialization phase
# give the user the opportunity to stop the programm

StartTime = time.time()
while True:
    print('LED on')
    led.on()
    sleep(1)
    print('LED off')
    led.off()
    sleep(1)
    if time.time()-StartTime > InitializationTime:
        break
print('Initialization done')


print('Launching main programm')

# launch audio process
AudioFileName = StartTimestamp + '_Audio.wav' 
p_audio = subprocess.Popen(['arecord',WaveFolder + AudioFileName,'--device=hw:0,0','--format','S16_LE','-c', '2','--rate','96000','--max-file-time=10'])


while True:
    Blink_nTimes(3)
    
    # get new IMU data
    if IMU.dataReady():
        IMU.getAgmt() # read all axis and temp from sensor, note this also updates all instance variables
        CSV_file = open(CsvFolder+StartTimestamp+'_data.csv', "a")
        CSV_file.write('TimeStampXXX,' + str(IMU.axRaw) + ',' + str(IMU.ayRaw) + ',' + str(IMU.azRaw) +'\n')
        CSV_file.close()
    
    # check folder for wav files to convert, sorted by name
    AudioFiles = sorted(glob.glob(WaveFolder + '*.wav'))
    # check if more than one file, if the convertion process is not already running
    if len(AudioFiles)>1 and ConvertToFlac and p_compress.poll() != None:
        # move first file (AudioFiles[0]) in list
        #os.rename(AudioFiles[0], FlacFolder +  os.path.basename(AudioFiles[0]))
        
        # lossless compression to FLAC format (warning: max 16 bit files)
        #os.system('flac -f ' + FlacFolder + os.path.basename(AudioFiles[0])) # Old, Blocking, replaced by Popen
        #p_compress = subprocess.Popen(['flac','-f','--delete-input-file',FlacFolder + os.path.basename(AudioFiles[0])])
        p_compress = subprocess.Popen(['flac','-f','--delete-input-file','--output-name='+ FlacFolder + os.path.basename(AudioFiles[0])+'.flac', WaveFolder + os.path.basename(AudioFiles[0])])
        
        # remove original wave, NOT USED because of '--delete-input-file' in flac conversion
        # os.remove(FlacFolder + os.path.basename(AudioFiles[0]))
    
    
    sleep(1)
    
    if time.time()-StartTime > MaxRunningTime:
        print('Maximum running time reached.')
        break
    
print('Programm finished, prepare to go to bed...')

# Stop the audio process
p_audio.terminate()

    
