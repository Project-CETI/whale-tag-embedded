#!/bin/bash

# micro-SD storage and battery periodic monitoring  
while :
do

# check the cell voltages and power off if needed. Hardcoded
# for now at 3V per cell.  

	v1=$(/opt/ceti-tag-battery-monitor/cetiTagBatteryStatus checkCell_1)
	v2=$(/opt/ceti-tag-battery-monitor/cetiTagBatteryStatus checkCell_2)
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
	  	cetiResponse=$(/opt/ceti-tag-battery-monitor/cetiTagBatteryStatus powerdown)
	  	cat $cetiResponse
		echo u > /proc/sysrq-trigger
		shutdown -P +1
		break	
	else 
	  	echo "battery is OK"
	fi

#loop timing
	sleep 60
done

# Some clean up on the way out...
echo "stopping cetiTagApp"
echo "quit" > /opt/ceti-tag-data-capture/ipc/cetiCommand
cat /opt/ceti-tag-data-capture/ipc/cetiResponse
sleep 15

# Optionally disable the service, must be reenabled when waking the tag up
# echo "disabling ceti-tag-data-capture service"
# systemctl disable ceti-tag-data-capture

echo "good night!"


