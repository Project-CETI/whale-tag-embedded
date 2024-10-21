#!/bin/bash

# New for 2.1-4 release  11/3/22
#
# If the tag is shutdown either buy running tagSleep.sh OR because the battery is less than about 6V
# this script can be run to renable charging and discharging

# Script assumes MAX17320 BMS is used, if using DS2778 uncomment the line below
# i2cset -y 1 0x59 0x00 0x03

# Write protection disabled
i2cset -y 1 0x36 0x61 0x0000 w
i2cset -y 1 0x36 0x61 0x0000 w
# Enable FETs
i2cset -y 1 0x36 0x61 0x0000 w
