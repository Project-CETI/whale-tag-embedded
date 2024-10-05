#! /bin/bash
# To start run ls -lA /dev/serial* and ensure /dev/serial0 -> ttyAMA0
# Instructions to fix: https://www.youtube.com/watch?v=tCcxFMU1OFE
# Script runtime is around 50s for recovery board firmware
# Run using sudo bash flashRecovery.sh <optionalarg>

# Configures the FPGA
# sudo /opt/ceti-tag-data-capture/bin/cetiFpgaInit
# code=$?
# if [ $code -ne 0 ]
# then
        echo 'Halting data capture...'
        sudo systemctl stop ceti-tag-data-capture
        # sudo /opt/ceti-tag-data-capture/bin/cetiFpgaInit
# fi

RF_PWR=2
BOOT0=1

# Set BOOT0 and RF_PWR as outputs 
CONFIG=$(i2cget -y 1 0x21 0x03) 
i2cset -y 1 0x21 0x3 $((CONFIG & ~(1 << RF_PWR) & ~(1 << BOOT0)))

# Initializes settings to power high, boot low
OUT=$(i2cget -y 1 0x21 0x01)
OUT=$((OUT | (1 << RF_PWR) & ~(1 << BOOT0)))
i2cset -y 1 0x21 0x1 $OUT

# Sets BOOT0 pin high
OUT=$((OUT | (1 << BOOT0)))
i2cset -y 1 0x21 0x1 $OUT
echo 'Boot pin has been set high.'

echo 'Resetting recovery board...'
# Resets board
OUT=$((OUT & ~(1 << RF_PWR)))
i2cset -y 1 0x21 0x1 $OUT
sleep 1
OUT=$((OUT | (1 << RF_PWR)))
i2cset -y 1 0x21 0x1 $OUT
sleep 1

# Checks connection to recovery board
stm32flash /dev/serial0 > /dev/null 2>&1
code=$?

if [[ $code -eq 1 ]]
then
        echo 'stm32flash cannot initialize device.'
        exit 1
fi

echo 'stm32 device has been initialized.'

# if [ -z "$1" ]; then
#         # Check for elf file existence
#         if ! [ -f /opt/ceti-tag-data-capture/ipc/flash.elf ]; then
#                 echo "File does not exist. Downloading..."
#                 # Download file
#                 curl -LJO "https://github.com/Project-CETI/whale-tag-recovery/raw/dev/v2_3/KaveetSakshamRecoveryBoard/KaveetSakshamRecoveryBoard.elf"
#                 sudo mv KaveetSakshamRecoveryBoard.elf /opt/ceti-tag-data-capture/ipc/flash.elf
#         fi

#         # Conversion from elf to binary
#         elf='/opt/ceti-tag-data-capture/ipc/flash.elf'
#         bin='/opt/ceti-tag-data-capture/ipc/flash.bin'
#         sudo objcopy -O binary $elf $bin
# elif
if [[  $1 == *.bin ]]; then
        bin="$1"
elif [[ $1 == *.elf ]]; then
        elf="$1"
        bin='/opt/ceti-tag-data-capture/ipc/flash.bin'
        sudo objcopy -O binary "$elf" $bin
else
        echo 'Invalid argument supplied'
        exit 1
fi


# Check that bin file exists
if ! test -f "$bin"; then
  echo "Bin file does not exist."
  exit 1
fi

# Flashes board
stm32flash -v -w "$bin" /dev/serial0
code=$?
sleep 1
if [ $code -eq 1 ]
then
        echo "Something went wrong when flashing stm32."
        exit 1
fi
echo 'Flashing stm32 has succeeded'

# Sets BOOT0 low to exit bootloader
OUT=$((OUT & ~(1 << BOOT0)))
i2cset -y 1 0x21 0x1 $OUT
echo 'Boot pin has been set low'

echo 'Resetting recovery board...'
# Resets board
OUT=$((OUT & ~(1 << RF_PWR)))
i2cset -y 1 0x21 0x1 $OUT
sleep 1
OUT=$((OUT | (1 << RF_PWR)))
i2cset -y 1 0x21 0x1 $OUT
sleep 1
