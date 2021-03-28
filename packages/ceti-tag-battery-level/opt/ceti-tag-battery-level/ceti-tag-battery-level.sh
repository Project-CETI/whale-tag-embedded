#!/bin/bash

LEVEL=`echo "get battery" | nc -q 0 127.0.0.1 8423 | sed s/"battery: "/""/`

# If battery level is less than 10%, do nothing
if [ 1 -eq "$(echo "10.0>${LEVEL}" | bc)" ]; then
    echo "10percent!"
    # Do something
fi

# If battery level is less than 5%, safely shutdown
if [ 1 -eq "$(echo "5.0>${LEVEL}" | bc)" ]; then
    echo "Battery has less than 5% left. Shutting down now."
    shutdown now
fi

#sleep a minute
sleep 60
