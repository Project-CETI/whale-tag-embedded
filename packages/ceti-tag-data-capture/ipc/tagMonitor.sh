#!/bin/bash

# New for 2.1-4 release  11/3/22

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

# If either cell is less than 3.00 V, power down the Pi
# and signal the FPGA to disable charging and discharging

	check1=$( echo "$v1 < 3.00" | bc )
	check2=$( echo "$v2 < 3.00" | bc )

	if [ $check1 -gt 0 ] || [ $check2 -gt 0 ]
	  then
	  	echo "low battery cell detected, powering Pi down now!"
	  	
# stopAcq will turn on red LED
	  	./tag.sh stopAcq	  	
	  	sleep 1

# powerdown signals FPGA to wait for Pi to turn off
# then disable charging and discharging. The entire
# tag will be turned off once the Pi shuts down.

# To subsequently enable the Tag requires connection of
# a charger and running the tagWake.sh script

	  	./tag.sh powerdown
	  	sleep 1
		
		shutdown -P now   

	  else 
	  	echo "battery is OK"
    fi

	sleep 60

done

