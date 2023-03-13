#!/bin/bash

# Battery periodic monitoring  

# Provide a startup grace period affords user a chance to stop 
# the battery monitoring service.  This is needed in the event that
# either cell is less than 3V after a deployment/recovery. Otherwise
# the system will go into a boot cycling loop

sleep 120

# Double check the cell voltages
# This is a new feature - require two sequential low voltage readings before
# declaring batteries have run out. That way, a single bad reading
# caused by any sort of i2c glitch and the like will be ignored.

cell_check_count=0

# Main loop: Periodically check the cell voltages and power the Tag off if needed. Hardcoded
# for now at 3V per cell. 

while :
do

	v1=$(/opt/ceti-tag-battery-monitor/bin/cetiTagBatteryStatus checkCell_1)
	v2=$(/opt/ceti-tag-battery-monitor/bin/cetiTagBatteryStatus checkCell_2)
	echo "cell voltages are $v1 $v2"

# If either cell is less than 3.00 V for two consecutive readings:
#    -signal the FPGA to disable charging and discharging (isolate the battery cells)
#    -schedule Pi shutdown
# When the Pi shuts down, the FPGA will detect a low-going GPIO and follow
# up with an i2c message to the BMS to disconnect the battery pack completely. The Tag will
# go dark with the batteries isolated.  The only way to turn it back on will
# be to connect to an external power supply or charger.

	check1=$( echo "$v1 < 3.00" | bc )
	check2=$( echo "$v2 < 3.00" | bc )

	if   [ $check1 -gt 0 ]  || [ $check2 -gt 0 ]; then
		if [ $cell_check_count -eq 0 ]; then
			cell_check_count=1
			echo "low cell detected - check again"
		elif [ $cell_check_count -eq 1 ]; then 
		  	echo "low battery cell detected, powering Pi down now!"
			echo s > /proc/sysrq-trigger
			cetiResponse=$(/opt/ceti-tag-battery-monitor/bin/cetiTagBatteryStatus powerdown)
	  		echo $cetiResponse
			echo u > /proc/sysrq-trigger
			shutdown -P +1
			break			
		fi 
	else
		cell_check_count=0
	  	echo "battery is OK"
	fi

#loop timing
	sleep 60

done

# Some clean up on the way out
echo "stopping cetiTagApp"
echo "quit" > /opt/ceti-tag-data-capture/ipc/cetiCommand
cat /opt/ceti-tag-data-capture/ipc/cetiResponse
sleep 5
echo "good night!"
