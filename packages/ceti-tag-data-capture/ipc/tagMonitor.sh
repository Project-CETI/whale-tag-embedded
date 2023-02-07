#!/bin/bash

# New for 2.1-4 release  11/5/22
# Supervisor script for the Tag application
# Updated 2/7/23 MRC - remove battery monitoring feature completely, will be done separately

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

# micro-SD storage monitoring - avoid full disk problems
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


