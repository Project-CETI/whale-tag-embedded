#!/bin/bash

# New for 2.1-4 release  11/5/22
# Supervisor script for the Tag application

# MRC changes for v2.3
#  - changed power down method for new BMS device 
#  - modified battery cutoff thresholds to be consistent with new BMS 

#handle SIGTERM from systemctl

# Get the location of this script, since will want to
#  use the pipes in that directory rather than the calling directory.
IPC_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PIPE_RESPONSE_TIMEOUT=5s
SLEEP_TIME=60
STARTUP_TIME=15
BURNWIRE_TIME=20m

## Forwards command to command pipe while performing error checking
send_command() {
  # check that app is running
  ps -p $child > /dev/null
  if [ $? -ne 0 ] ;  then 
    echo "cetiTagApp is not running"
    return $ERR_NO_CHILD
  fi

  # send response to pipe
  echo "$*" > $IPC_DIR/cetiCommand
  if [ $? -ne 0 ] ; then
    echo "Failed to send command to cetiCommand pipe"
    return $ERR_UNRESPONSIVE_CHILD
  fi

  # read return
  response=$(timeout $PIPE_RESPONSE_TIMEOUT cat $IPC_DIR/cetiResponse)
  if [ $? -ne 0 ] ; then
    echo "Failed to receive command response in timely manor"
    return $ERR_UNRESPONSIVE_CHILD
  fi
  
  echo $response
  return 0
}

## Perform shutdown without assistance of cetiTagApp
emergency_shutdown() {
  echo "Initiating FPGA shutdown"
  SHUTDOWN_COMMAND=(0x02 0x0E 0x6C 0x61 0x03 0x00 0x00 0x03)
  FPGA_CAM_RESET=5
  FPGA_CAM_DOUT=18
  FPGA_CAM_SCK=16
  raspi-gpio set FPGA_CAM_RESET dh op
  raspi-gpio set FPGA_CAM_SCK dl op
  raspi-gpio set FPGA_CAM_DOUT dl op
  for byte in ${SHUTDOWN_COMMAND[@]}
  do
    for bit in {0..7}
    do
      if [[ "$((byte & (1 <<(7 - bit))))" == "0" ]]
      then
        raspi-gpio set FPGA_CAM_DOUT dl
      else
        raspi-gpio set FPGA_CAM_DOUT dh 
      fi
      sleep 0.1
      raspi-gpio set FPGA_CAM_SCK dh
      sleep 0.1
      raspi-gpio set FPGA_CAM_SCK dl
      sleep 0.1
    done
  done
  for byte in {0..7}
  do
    for bit in {0..7}
    do
      raspi-gpio set FPGA_CAM_SCK dh
      sleep 0.1
      raspi-gpio set FPGA_CAM_SCK dl
      sleep 0.1
    done
  done

  echo "Control Handed over to FPGA"
  echo "Good bye!"
  shutdown now
  exit 1
}

## Attempt tag burnwire release and shutdown without assistance of cetiTagApp
emergency_release() {
  echo "Unable to restart cetiTagApp"
  echo "Starting emergency shutdown"
  
  echo "Activating burnwire"
  IOX_CONFIG=i2cget -y 1 0x21 0x03
  if [ $? -eq 0 ] ; then
    # turn on burnwire
    i2cset -y 1 0x21 0x03 $((IOX_CONFIG & ~(1 << 4)))
    IOX_OUT=i2cget -y 1 0x21 0x01
    IOX_OUT=$((IOX_OUT | (1 << 4)))
    i2cset -y 1 0x21 0x01 $IOX_OUT

    sleep $BURNWIRE_TIME # Do we sleep here?
    # turn off burnwire
    IOX_OUT=$((IOX_OUT & ~(1<<4)))
    i2cset -y 1 0x21 0x01 $IOX_OUT  
  else
    echo "ERR: could not communicate with burnwire"
  fi

  emergency_shutdown
}

ERR_NO_CHILD=1
ERR_UNRESPOSIVE_CHILD=2
## Handle common errors
handle_error() {
  case $1 in
    $ERR_UNRESPONSIVE_CHILD)
      echo "Err: unresponsive child"
      echo "Killing process $child"
      kill -9  "$child" 2>/dev/null
      ;& #fallthrough
    
    $ERR_NO_CHILD)
      echo "Err: no child"
      echo "Attempting to restart child process"
      # start child
      /opt/ceti-tag-data-capture/bin/cetiTagApp &
      child=$! # note pid
      sleep $STARTUP_TIME # wait for app to boot
      data_acquisition_running=1
      #check that app is still running
      ps -p $child > /dev/null
      if [ $? -ne 0 ] ; then
        echo "Err: Failed to restart child process"
        emergency_release
      fi
      ;;
  esac
}

_term() {
  echo "stopping cetiTagApp"
  send_command quit
  if [ $? -ne 0 ] ; then
    handle_error $?
  fi

  kill --TERM  "$child" 2>/dev/null

  ## Wait for cetiTagApp to finish
  echo "Waiting on child process $child to finish..."
  wait "$child"

  echo "Child process $child complete"
  echo "Good bye"
  exit 0;
}

trap _term SIGTERM

# # remount rootfs readonly
mount /boot -o remount,ro

./tagWake.sh

# Launch the main recording application in the background.
/opt/ceti-tag-data-capture/bin/cetiTagApp &
child=$!

# Wait for the main loops to start so it is ready to receive commands.
sleep $STARTUP_TIME
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
    send_command stopDataAcq
    if [ $? -ne 0 ] ; then
      handle_error $?
      continue
    fi
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
  sleep $SLEEP_TIME

# check the cell voltages and power off if needed.
# Hardcoded for now at 3V per cell.

#check that process is active before calling into pipe
  v1=$(send_command battery cellV 0)
  if [ $? -ne 0 ] ; then
    handle_error $?
    continue
  fi

  v2=$(send_command battery cellV 1)
  if [ $? -ne 0 ] ; then
    handle_error $?
    continue
  fi

  iMa=$(send_command battery current)
  if [ $? -ne 0 ] ; then
    handle_error $?
    continue
  fi

  echo "cell voltages are [ $v1, $v2 ]V @ $iMa mA"

# Check if discharging
  is_discharging=$( echo "$iMa < 0.0" | bc )
  if [ "$is_discharging" -gt 0 ]
  then
    echo "Batteries are discharging"

# If either cell is less than 3.20 V:
#    - stop data acquisition to gracefully exit the thread loops and close files
    check1=$( echo "$v1 < 3.20" | bc )
    check2=$( echo "$v2 < 3.20" | bc )

    if [ "$check1" -gt 0 ] || [ "$check2" -gt 0 ] 
    then
      echo "low battery cell detected; stopping data acquisition"
      send_command stopDataAcq
      if [ $? -ne 0 ] ; then
        handle_error $?
        continue
      fi
      echo "signaled to stop data acquisition"
      data_acquisition_running=0
    else
        echo "battery is OK"
    fi
    

# If either cell is less than 3.10 V:
#    -signal the FPGA to disable charging and discharging
#    -schedule Pi shutdown

    check1=$( echo "$v1 < 3.10" | bc )
    check2=$( echo "$v2 < 3.10" | bc )

    if [ "$check1" -gt 0 ] || [ "$check2" -gt 0 ] 
    then
      echo "low battery cell detected; powering down the Pi now!"
      echo s > /proc/sysrq-trigger
      send_command powerdown
      if [ $? -ne 0 ] ; then
        kill -9  "$child" 2>/dev/null
        emergency_shutdown
      fi

      echo u > /proc/sysrq-trigger
      shutdown -P +1
      break  
    else
        echo "battery is OK"
    fi
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
    touch /data/tagMonitor_log_fifo_overflows.csv
    echo -n "$(date '+%s,%F %H-%M-%S')" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
    echo ", fault detected" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
    # Wait to see if the tag software handles it.
    echo "FIFO overflow detected, waiting for the main app to handle it"
    sleep 30
    overflow=$(pigs r 12)
    if [ "$overflow" -eq 1 ]q
    then
      echo "FIFO overflow still detected, restarting the main app"
      # Tell the main app to gracefully quit.
      send_command stopDataAcq
      if [ $? -ne 0 ] ; then
        handle_error $?
        continue
      fi
      echo -n "$(date '+%s,%F %H-%M-%S')" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      echo ", still overflow - sent quit command" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      sleep 30
      # Make sure the app is stopped.
      pkill cetiTagApp
      echo -n "$(date '+%s,%F %H-%M-%S')" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      echo ", killed app process" | sudo tee -a /data/tagMonitor_log_fifo_overflows.csv
      sleep 5
      # Start the main app again.
      /opt/ceti-tag-data-capture/bin/cetiTagApp &
      child=$! # Reattach child process
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
_term
