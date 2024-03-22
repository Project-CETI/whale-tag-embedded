
#! /bin/bash
# To start run ls -lA /dev/serial* and ensure /dev/serial0 -> ttyAMA0
# Instructions to fix: https://www.youtube.com/watch?v=tCcxFMU1OFE
# Script runtime is around 50s for recovery board firmware

# Configures the FPGA
sudo /opt/ceti-tag-data-capture/bin/cetiFpgaInit
code=$?
if [ $code -ne 0 ]
then
        echo 'stopped capture'
        sudo systemctl stop ceti-tag-data-capture
        sudo /opt/ceti-tag-data-capture/bin/cetiFpgaInit
fi

# Sets BOOT0 pin high
x=$(i2cget -y 1 0x38)
i2cset -y 1 0x38 $((x|0x04))
echo 'boot high'

echo 'reset board'
# Resets board
raspi-gpio set 13 op dl
sleep 1
raspi-gpio set 13 dh
sleep 1

# Checks connection to recovery board
stm32flash /dev/serial0
code=$?
counter=0

while [ $counter -le 5 ] && [ $code -eq 1 ]; do
	echo $counter
        raspi-gpio set 13 dl
        sleep 2
        raspi-gpio set 13 dh
        sleep 2
        stm32flash /dev/serial0
        code=$?
        counter=$((counter+1))
done

if [ $code -eq 1 ]
then
        echo "stm32flash cannot initialize device."
        exit 1
fi

echo 'initialized device'

# Download file
curl -o flash.elf https://github.com/Project-CETI/whale-tag-recovery/blob/d104ac37ef99f0f08ca0998f8e62776b750c8978/KaveetSakshamRecoveryBoard/KaveetSakshamRecoveryBoard.elf

# Conversion from elf to binary
elf=${1:-'flash.elf'}
bin='./flash.bin'
objcopy -O binary "./${elf}" $bin
code=$?
if [ $code -eq 1 ]
then
        echo "elf file not found."
        exit 1
fi

# Flashes board
stm32flash -v -w $bin /dev/serial0
code=$?
sleep 1
if [ $code -eq 1 ]
then
        echo "Something went wrong when flashing."
        exit 1
fi
echo 'flash succeeded'

echo 'boot low'
# Sets BOOT0 low to exit bootloader
i2cset -y 1 0x38 $((x&0xfb))

echo 'reset'
# Resets board
raspi-gpio set 13 dl
sleep 1
raspi-gpio set 13 dh
sleep 1
