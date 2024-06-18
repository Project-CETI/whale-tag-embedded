#!/bin/bash

# New for 2.1-4 release  11/3/22
#
# This script disables charging and discharging
# Use for long-term storage or shipping 

i2cset -y 1 0x59 0x00 0x00




