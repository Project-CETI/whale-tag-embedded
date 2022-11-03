#!/bin/bash

# New for 2.1-4 release  11/3/22
#
# If the tag is shutdown either buy running tagSleep.sh OR because the battery is less than about 6V
# this script can be run to renable charging and discharging

i2cset -y 1 0x59 0x00 0x03




