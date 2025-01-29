#!/bin/bash
# This script allows non volatile BMS writes to be done via shell instead of C
# The numbers correspond to the steps on page 125 of https://www.analog.com/media/en/technical-documentation/data-sheets/MAX17320.pdf

# 1
i2cset -y 1 0x36 0x61 0x0000 w
i2cset -y 1 0x36 0x61 0x0000 w
# 2
# See BMS NV Write Spreadsheet for explanations
i2cset -y 1 0x0b 0xcf 0x03e8 w # MAX17320_REG_NRSENSE
i2cset -y 1 0x0b 0xb3 0x2710 w # MAX17320_REG_NDESIGNCAP
i2cset -y 1 0x0b 0xb5 0xc208 w # MAX17320_REG_NPACKCFG
i2cset -y 1 0x0b 0xb8 0x0830 w # MAX17320_REG_NNVCFG0
i2cset -y 1 0x0b 0xb9 0x2100 w # MAX17320_REG_NNVCFG1
i2cset -y 1 0x0b 0xba 0x822d w # MAX17320_REG_NNVCFG2
i2cset -y 1 0x0b 0xd0 0xa002 w # MAX17320_REG_NUVPRTTH
i2cset -y 1 0x0b 0xd1 0x280a w # MAX17320_REG_NTPRTTH1
i2cset -y 1 0x0b 0xd3 0x32ce w # MAX17320_REG_NIPRTTH1 #set new slow current limits for development (2A)
i2cset -y 1 0x0b 0xd4 0x0ca0 w # MAX17320_REG_NBALTH   #disable imbalance charge term for development
i2cset -y 1 0x0b 0xd6 0x0813 w # MAX17320_REG_NPROTMISCTH
i2cset -y 1 0x0b 0xd7 0x0c08 w # MAX17320_REG_NPROTCFG
i2cset -y 1 0x0b 0xd9 0xec00 w # MAX17320_REG_NJEITAV
i2cset -y 1 0x0b 0xda 0xb3a0 w # MAX17320_REG_NOVPRTTH
i2cset -y 1 0x0b 0xdc 0x0035 w # MAX17320_REG_NDELAYCFG
i2cset -y 1 0x0b 0xde 0x4058 w # MAX17320_REG_NODSCCFG
i2cset -y 1 0x0b 0xb0 0x0290 w # MAX17320_REG_NCONFIG
i2cset -y 1 0x0b 0xca 0x71be w # MAX17320_REG_NTHERMCFG
i2cset -y 1 0x0b 0x9e 0x9659 w # MAX17320_REG_NVEMPTY
i2cset -y 1 0x0b 0xc6 0x5005 w # MAX17320_REG_NFULLSOCTHR

# 3
i2cset -y 1 0x36 0x61 0x0000 w
# 4
i2cset -y 1 0x36 0x60 0xE904 w
# 5
sleep 10
# 6
x=$(i2cget -y 1 0x36 0x61 w)
y=$((x & 0x0004))
if ((y == 4)); then
	echo 'NV Error Bit not cleared, try again'
	exit 1
fi
# 7
i2cset -y 1 0x36 0x60 0x000F w
# 8
sleep .1
# 9
i2cset -y 1 0x36 0x61 0x0000 w
i2cset -y 1 0x36 0x61 0x0000 w
# 10
i2cset -y 1 0x36 0xAB 0x8000 w
# 11
sleep 1
x=$(i2cget -y 1 0x36 0xAB w)
y=$((x & 0x8000))
if ((y == 0x8000)); then
	echo 'POR Process not complete. Wait, then clear write protection'
	exit 0
fi
# 12
i2cset -y 1 0x36 0x61 0x00f9 w
i2cset -y 1 0x36 0x61 0x00f9 w
echo 'Nonvolitile memory set'
exit 0
