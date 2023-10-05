#!/bin/bash

# New for 2.1-4 release  11/5/22
# Supervisor script for the Tag application

# remount rootfs readonly
mount /boot -o remount,ro


# Check that there is some disk space available before starting the recorder
	avail=$(df --output=target,avail | grep /data)
	echo $avail
	set $avail
	echo $2
	if [ $2 -lt 1000000 ] 
	then
    	echo "disk almost full, cannot start recording"
    	exit   		 
	 else	 	
	 	echo "disk space is OK - starting the recorder"
	 	disk_full=0
	fi

# Provide a startup delay to allow a user to connect before recorder begins using resources
sleep  30

# Launch the main recording application in the background
sudo /opt/ceti-tag-data-capture/bin/cetiTagApp & 

# micro-SD storage and battery periodic monitoring  
while :
do

# Check that there is disk space available to continue recording	
	avail=$(df --output=target,avail | grep /data)
#	echo $avail
	set $avail
	echo "$2 KiB available" 

# The tags are normally built with 128 GB uSD cards.  Leave 1 GB headroom
# Note - sensors.csv will continue, just audio is stopped.  

	if [ $2 -lt 1000000 ] &&  [  $disk_full -eq 0 ] 
	then
    	echo "disk almost full, suspend audio recording"
		echo "stopAcq" > cetiCommand
                sleep 1
                cat cetiResponse
                echo "audio recording suspended successfully"
		disk_full=1

  	 elif  [ $disk_full -eq 1 ] 
  	 then
		echo "disk full, recording suspended"
	 
	 else
	 	echo "disk space is OK"

	fi

# check the cell voltages and power off if needed. Hardcoded
# for now at 3V per cell.  

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

	if [ $check1 -gt 0 ] || [ $check2 -gt 0 ] 
	then
	  	echo "low battery cell detected, powering Pi down now!"
		echo s > /proc/sysrq-trigger
	  	echo "powerdown" > cetiCommand
	  	cat cetiResponse
		echo u > /proc/sysrq-trigger
		shutdown -P +1
		break	
	 else 
	  	echo "battery is OK"
	fi

# Check for overflow.  
# If overflow has occurred, restart the main app to cleanly reset
# The overflow flag is the Pi GPIO 12, read using pigpio utility
# A note is made in the /data/fifo_status.txt file if there is an event

#	overflow=$(pigs r 12)
#
#	if [ $overflow -eq 1 ]
#	then
#		echo "FIFO overflow detected, restarting recorder"
#		echo "quit" > cetiCommand
#		cat cetiResponse
#		sudo touch /data/fifo_status.txt
#        date | sudo tee -a /data/fifo_status.txt
#		echo "fault detected" | sudo tee -a /data/fifo_status.txt
#		sleep 30
#		sudo /opt/ceti-tag-data-capture/bin/cetiTagApp &
#	else
#		echo "FIFO is OK"
#	fi

#loop timing
	sleep 60
done

# Some clean up on the way out...
echo "stopping cetiTagApp"
echo "quit" > cetiCommand
cat cetiResponse
sleep 15

# Optionally disable the service, must be reenabled when waking the tag up
# echo "disabling ceti-tag-data-capture service"
# systemctl disable ceti-tag-data-capture

echo "good night!"

