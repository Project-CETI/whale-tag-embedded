#! /bin/bash
# To start run ls -lA /dev/serial* and ensure /dev/serial0 -> ttyAMA0
# Instructions to fix: https://www.youtube.com/watch?v=tCcxFMU1OFE
# Script runtime is around 50s for recovery board firmware

# Configures the FPGA
sudo /opt/ceti-tag-data-capture/bin/cetiFpgaInit

# Sets BOOT0 pin high
x=$(i2cget -y 1 0x38)
i2cset -y 1 0x38 $((x|0x04))

# Resets board
raspi-gpio set 13 op dl
sleep 1
raspi-gpio set 13 dh
sleep 1

# Checks connection to recovery board
stm32flash /dev/serial0 > /dev/null 2>&1
code=$?
if [ $code -eq 1 ]
then
	echo "stm32flash cannot initialize device."
	exit 1
fi

# Conversion from elf to binary
elf=${1:-'KaveetSakshamRecoveryBoard.elf'}
bin='./flash.bin'
objcopy -O binary "./${elf}" $bin

# Flashes board
stm32flash -v -w $bin /dev/serial0 > /dev/null 2>&1
sleep 1
code=$?
if [ $code -eq 1 ]
then
	echo "Something went wrong when flashing."
	exit 1
fi

# Sets BOOT0 low to exit bootloader
i2cset -y 1 0x38 $((x&0xfb))

# Resets board
raspi-gpio set 13 dl
sleep 1
raspi-gpio set 13 dh
sleep 1
