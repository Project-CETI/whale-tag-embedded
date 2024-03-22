#! /bin/bash
# To start run ls -lA /dev/serial* and ensure /dev/serial0 -> ttyAMA0
# Instructions to fix: https://www.youtube.com/watch?v=tCcxFMU1OFE
# Script runtime is around 50s for recovery board firmware

# Configures the FPGA
sudo /opt/ceti-tag-data-capture/bin/cetiFpgaInit
code=$?
if [ $code -ne 0 ]
then
        echo 'Data capture has been halted.'
        sudo systemctl stop ceti-tag-data-capture
        sudo /opt/ceti-tag-data-capture/bin/cetiFpgaInit
fi

# Sets BOOT0 pin high
x=$(i2cget -y 1 0x38)
i2cset -y 1 0x38 $((x|0x04))
echo 'Boot pin has been set high.'

echo 'Resetting recovery board...'
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
        echo 'stm32flash cannot initialize device.'
        exit 1
fi

echo 'stm32 device has been initialized.'

# Check for elf file existence
if ! [ -f /opt/ceti-tag-data-capture/ipc/flash.elf ]; then
        echo "File does not exist. Downloading..."
        # Download file
        curl -LJO "https://github.com/Project-CETI/whale-tag-recovery/raw/dev/v2_3/KaveetSakshamRecoveryBoard/KaveetSakshamRecoveryBoard.elf"
        sudo mv KaveetSakshamRecoveryBoard.elf /opt/ceti-tag-data-capture/ipc/flash.elf
fi

# Conversion from elf to binary
elf='/opt/ceti-tag-data-capture/ipc/flash.elf'
bin='/opt/ceti-tag-data-capture/ipc/flash.bin'
sudo objcopy -O binary $elf $bin

# Flashes board
stm32flash -v -w $bin /dev/serial0
code=$?
sleep 1
if [ $code -eq 1 ]
then
        echo "Something went wrong when flashing stm32."
        exit 1
fi
echo 'Flashing stm32 has succeeded'

# Sets BOOT0 low to exit bootloader
i2cset -y 1 0x38 $((x&0xfb))
echo 'Boot pin has been set low'

echo 'Resetting recovery board...'
# Resets board
raspi-gpio set 13 dl
sleep 1
raspi-gpio set 13 dh
sleep 1
