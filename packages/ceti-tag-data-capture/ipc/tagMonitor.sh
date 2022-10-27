#!/bin/bash

#

# Provide a startup delay to allow a user to connect before recorder begins using resources 
sleep 30

# Launch the main recording application in the background
sudo /opt/ceti-tag-data-capture/bin/cetiTagApp & 

#----------------------------------------------
# micro-SD storage monitoring  

while :
do
	avail=$(df --output=source,avail | grep /dev/root)
	echo $avail
	set $avail
	echo $2

	if [ $2 -lt 22000000 ]
  	  then 
    		echo "disk almost full, stopping the service"
    		systemctl stop ceti-tag-data-capture
  	  else 
    		echo "disk space OK"
	fi

	sleep 60

done

#----------------------------------------------
