#!/bin/bash

# Provide a startup delay to allow a user to connect before recorder begins using resources 
sleep 30

# Launch the main recording application in the background
sudo /opt/ceti-tag-data-capture/bin/cetiTagApp & 

# micro-SD storage and battery periodic monitoring  
while :
do

# check that there is disk space available to continue recording	
	avail=$(df --output=source,avail | grep /dev/root)
	echo $avail
	set $avail
	echo $2

# The tags are normally built with 128 GB uSD cards.  Leave 1 GB headroom

	if [ $2 -lt 1000000 ]
  	  then 
    		echo "disk almost full, stopping the service"
    		systemctl stop ceti-tag-data-capture
  	  else 
    		echo "disk space OK"
	fi

# check the cell voltages and power off if needed. Hardcoded
# for now at 3V per cell.  

# use named pipe to grab cell voltages from the recorder app
	v1=$(./tag.sh checkCell_1)
	v2=$(./tag.sh checkCell_2)
	echo "cell voltages $v1 $v2"

# compare with critical voltage - if either cell below critical
# stop the recorder application and power down the Pi
# the Tag will still take some power until the battery UV protection kicks in
# at 2.6 V.  This is an interim
# solution that as a minimum helps avoid file system corruption. 
# Ideally battery discharge is disabled entirely but
# that requires a more sophisticated approach that involves either
# having the FPGA send commands to the protector OR somehow carefully
# managing the file system then sending the pull the plug command 
# from the Pi.  A bit tricky and there is a lot to improve here....

	check1=$( echo "$v1 < 2.99" | bc )
	check2=$( echo "$v2 < 2.99" | bc )

	if [ $check1 -gt 0 ] || [ $check2 -gt 0 ]
	  then
	  	echo "low battery cell detected, powering Pi down now!"
	  	./tag.sh stopAcq
	  	sleep 1
		systemctl poweroff
	  else 
	  	echo "battery is OK"
    fi

	sleep 60

done

