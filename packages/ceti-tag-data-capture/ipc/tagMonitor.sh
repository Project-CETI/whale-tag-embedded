#!/bin/bash

# New for 2.1-4 release  11/5/22
# Supervisor script for the Tag application

# remount rootfs readonly
mount /boot -o remount,ro

#wake the tag (if )
./tagWake.sh

# Launch the main recording application in the background.
sudo /opt/ceti-tag-data-capture/bin/cetiTagApp &
# Wait for the main loops to start so it is ready to receive commands.
sleep 15
data_acquisition_running=1

# Periodically monitor the battery, SD storage, and audio overflows.
while :
do
# Check that there is disk space available to continue recording.
# The tag app should automatically stop if there is less than 1 GiB free.
  disk_avail_kb=$(df --output=target,avail | grep /data)
  set $disk_avail_kb
  echo "$2 KiB available"
  if [ $data_acquisition_running -eq 1 ] && [ $2 -lt $((1*1024*1024)) ]
  then
    echo "disk almost full; stopping data acquisition"
    echo "stopDataAcq" > cetiCommand
    sleep 1
    cat cetiResponse
    echo "signaled to stop data acquisition"
    data_acquisition_running=0
  elif [ $data_acquisition_running -eq 0 ]
  then
    echo "disk full; data acquisition was previously signaled to stop"
  else
    echo "disk space is OK"
  fi

#loop timing and initial delay so the user has time to log in before the
#script shuts the system down
  sleep 60

# check the cell voltages and power off if needed.
# Hardcoded for now at 3V per cell.

  echo "checkCell_1" > cetiCommand
  v1=$(cat cetiResponse)
  echo "checkCell_2" > cetiCommand
  v2=$(cat cetiResponse)

  echo "cell voltages are $v1 $v2"

# If either cell is less than 3.00 V:
#    -signal the FPGA to disable charging and discharging
#    -schedule Pi shutdown

  check1=$( echo "$v1 < 3.00" | bc )
  check2=$( echo "$v2 < 3.00" | bc )

  if [ "$check1" -gt 0 ] || [ "$check2" -gt 0 ] 
  then
    echo "low battery cell detected; powering down the Pi now!"
    echo s > /proc/sysrq-trigger
    echo "powerdown" > cetiCommand
    cat cetiResponse
    echo u > /proc/sysrq-trigger
    shutdown -P +1
    break  
  else
      echo "battery is OK"
  fi

# Check for audio FIFO overflow.
#   If overflow has occurred, first see if the main app successfully handles the error.
#   If it doesn't, restart the main app to completely reset.
# The overflow flag is the Pi GPIO 12, read using pigpio utility
# A note is made in the /data/tagMonitor_log_fifo_overflows.csv file if there is an event

  overflow=$(pigs r 12)
  if [ $data_acquisition_running -eq 1 ] && [ "$overflow" -eq 1 ]
  then
    # Log the event.
    sudo touch /data/tagMonitor_log_fifo_overflows.csv
    echo -n "$(date '+%s,%F %H-%M-%S')" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
    echo ", fault detected" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
    # Wait to see if the tag software handles it.
    echo "FIFO overflow detected, waiting for the main app to handle it"
    sleep 30
    overflow=$(pigs r 12)
    if [ "$overflow" -eq 1 ]
    then
      echo "FIFO overflow still detected, restarting the main app"
      # Tell the main app to gracefully quit.
      echo "quit" > cetiCommand
      cat cetiResponse
      echo -n "$(date '+%s,%F %H-%M-%S')" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      echo ", still overflow - sent quit command" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      sleep 30
      # Make sure the app is stopped.
      sudo pkill cetiTagApp
      echo -n "$(date '+%s,%F %H-%M-%S')" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      echo ", killed app process" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      sleep 5
      # Start the main app again.
      sudo /opt/ceti-tag-data-capture/bin/cetiTagApp &
      echo -n "$(date '+%s,%F %H-%M-%S')" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      echo ", main app started" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
    else
      echo "FIFO is OK; the main app handled the overflow"
      echo -n "$(date '+%s,%F %H-%M-%S')" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      echo ", main app handled overflow" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
    fi
  elif [ $data_acquisition_running -eq 1 ]
  then
    echo "FIFO is OK; no overflow detected"
  fi
done

# Some cleanup on the way out...
echo "stopping cetiTagApp"
echo "quit" > cetiCommand
cat cetiResponse
sleep 120

# Optionally disable the service, must be reenabled when waking the tag up
# echo "disabling ceti-tag-data-capture service"
# systemctl disable ceti-tag-data-capture

echo "good night!"

