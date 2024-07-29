#!/bin/bash

# New for 2.1-4 release  11/3/22
#
# This script disables charging and discharging
# Use for long-term storage or shipping 

# Script assumes MAX17320 BMS is used, if using DS2778 uncomment the line below
# i2cset -y 1 0x59 0x00 0x00

# Write protection disabled
i2cset -y 1 0x36 0x61 0x0000 w
i2cset -y 1 0x36 0x61 0x0000 w
# Disable FETs
i2cset -y 1 0x36 0x61 0x0300 w




