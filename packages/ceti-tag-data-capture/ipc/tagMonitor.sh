#!/bin/bash

# New for 2.1-4 release  11/5/22
# Supervisor script for the Tag application
# 
# 12/15/22 MRC 
# Updated with additional start up delay to accomodate read-only boot time

# Updated 3/11/23  Remove battery monitor feature, will be handled by separate service

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

# Add another 2 minute holdoff before routine checking starts. This provides a window during
# which the operator can intervene with the automatic shutdown in cases where the battery
# has been deeply discharged 
sleep  120

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

#loop timing
	sleep 60
done

