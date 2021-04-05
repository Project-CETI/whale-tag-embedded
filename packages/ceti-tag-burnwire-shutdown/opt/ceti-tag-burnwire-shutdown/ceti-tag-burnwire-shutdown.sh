#!/bin/bash

# MAXUPTIME_S is the maximum number of seconds 
# the system is allowed to stay operational.
# After the MAXUPTIME_S uptime is reached, the
# system will engage the burnwire and shutdown.
# Default value is "-1", signifying no limit.
MAXUPTIME_S="-1"

# If the current battery level is below 
# SHUTDOWN_BATTERY_LEVEL, engage the burnwire
# and safely shutdown the system.
SHUTDOWN_BATTERY_LEVEL="5.0"

# GPIO15 is the burnwire. Default level is low.
setup_burnwire_gpio () {
    echo "15" > /sys/class/gpio/export                  
    echo "out" > /sys/class/gpio/gpio15/direction
    echo "0" > /sys/class/gpio/gpio15/value
}

# To engage the burnwire, one needs to drive it high for 20 seconds.
engage_burnwire_and_shutdown () {
    # Stop data collection
    systemctl stop ceti-tag-data-capture.service
    # Engage burnwire
    echo "1" > /sys/class/gpio/gpio15/value
    sleep 20
    echo "0" > /sys/class/gpio/gpio15/value
    # Safely shutdown
    echo "Shutting down now."
    shutdown now
}

# Make sure the GPIO 15 is setup
setup_burnwire_gpio

# Loop forever
while :
do

    # Check battery level
    # Engage burnwire and shutdown if battery level below ${SHUTDOWN_BATTERY_LEVEL}
    LEVEL="$(echo "get battery" | nc -q 0 127.0.0.1 8423 | sed s/"battery: "/""/)"
    if [ -n "$LEVEL" ]; then
        if [ 1 -eq "$(echo "${SHUTDOWN_BATTERY_LEVEL}>${LEVEL}" | bc)" ]; then
            echo "Battery has less than 5% left. Engaging burnwire."
            engage_burnwire_and_shutdown
        fi
    fi

    # If the maximum system uptime has been set,
    # get the current uptime and compare
    if [ 1 -eq "$(echo "${MAXUPTIME_S}>0" | bc)" ]; then
        # If the uptime limit has been set and reached, 
        # engage burnwire and safely shutdown
        UPTIME_S="$(awk '{print $1;}' < /proc/uptime)"
        if [ 1 -eq "$(echo "${UPTIME_S}>${MAXUPTIME_S}" | bc)" ]; then
            echo "Maximum uptime of ${MAXUPTIME_S} seconds reached. Engaging burnwire."
            engage_burnwire_and_shutdown
        fi

    fi

    # sleep a minute
    sleep 60

done

