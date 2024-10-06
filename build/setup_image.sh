#!/bin/bash
set -e

if [ "$#" -ne 1 ]; then
	echo "$0 <overlay-dir>" >&2
	exit 1
fi

OVERLAY_DIR="$1"

APT_NONINTERACTIVE="-y"
export DEBIAN_FRONTEND="noninteractive"
export APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=yes

apt update

time apt install "${APT_NONINTERACTIVE}" --fix-broken --fix-missing --no-upgrade \
	alsa-utils \
	avahi-utils \
	bc \
	build-essential \
	libpigpio-dev \
	pigpio \
	devscripts \
	dkms \
	dnsmasq \
	flac \
	libflac-dev \
	libi2c-dev \
	i2c-tools \
	netcat \
	zlib1g-dev

apt "${APT_NONINTERACTIVE}" autoremove

# get current date
date -s "$(curl -s --head http://google.com | grep ^Date: | sed 's/Date: //g')"

# Isolate CPUs that will be used for audio and ECG capture,
#  so kernel/system processes are scheduled on other cores.
sed -i '$ s/$/ isolcpus=2,3/' /boot/cmdline.txt

# Disable rfkill state restore and set default state to wifi on
sed -i '$ s/$/ systemd.restore_state=0/' /boot/cmdline.txt
sed -i '$ s/$/ rfkill.default_state=1/' /boot/cmdline.txt

# Setup hardware parameters for i2c
raspi-config nonint do_i2c 0
{
	echo "dtparam=i2c_vc=on"
	echo "dtparam=i2c_vc_baudrate=400000"
	echo "dtparam=i2c_arm_baudrate=400000"
} >>/boot/config.txt

# Setup UART for flashing recovery board
echo "dtoverlay=miniuart-bt" >>/boot/config.txt

# Set timezone
raspi-config nonint do_change_timezone "America/Dominica"

# Set wifi country
raspi-config nonint do_wifi_country US

# Update keyboard layout.
sed -i -e 's/"gb"/"us"/' /etc/default/keyboard

# Keep the original pi system user around
/usr/bin/cancel-rename pi

# Change default password
echo -e "ceticeti\nceticeti" | passwd pi

# Create /data folder - that's where the captured data will go
mkdir -p /data
chmod 777 /data

# Add entry in fstab to mount a data partition to /data by label if present
echo "/dev/disk/by-label/cetiData /data ext4 defaults,nofail 0 0" >>/etc/fstab

# Do not run pi wizard
rm -rf /etc/xdg/autostart/piwiz.desktop

# Disable first boot rootfs resize
rm /etc/init.d/resize2fs_once

# Copy filesystem overlay.
tar -cf - -C "${OVERLAY_DIR}" --owner=pi --group=pi . | tar -xf - -C /

# Disable periodic systemd services
rm -f /etc/systemd/system/timers.target.wants/apt-daily.timer
rm -f /etc/systemd/system/timers.target.wants/apt-daily-upgrade.timer
rm -f /etc/systemd/system/timers.target.wants/man-db.timer

# Add useful commands to the bash history.
rm -f /home/pi/.bash_history
dos2unix /usr/lib/raspberrypi-sys-mods/custom_bash_history.txt
mv /usr/lib/raspberrypi-sys-mods/custom_bash_history.txt /home/pi/.bash_history

# Sourceforge install of stm32flash to get 0.7
git clone https://git.code.sf.net/p/stm32flash/code stm32flash-code
make install -C stm32flash-code -j4
rm -rf stm32flash-code
