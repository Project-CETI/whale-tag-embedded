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
	avahi-utils \
	bc \
	build-essential \
	cryptsetup \
	cryptsetup-bin \
	devscripts \
	dkms \
	dnsmasq \
	flac \
	i2c-tools \
	libflac-dev \
	libi2c-dev \
	libpigpio-dev \
	overlayroot \
	pigpio \
	rsyslog

apt "${APT_NONINTERACTIVE}" autoremove

# get current date
date -s "$(curl -s --head http://google.com | grep ^Date: | sed 's/Date: //g')"

# Isolate CPUs that will be used for audio and ECG capture,
#  so kernel/system processes are scheduled on other cores.
sed -i '$ s/$/ isolcpus=2,3/' /boot/cmdline.txt

# Disable rfkill state restore and set default state to wifi on
sed -i '$ s/$/ systemd.restore_state=0/' /boot/cmdline.txt
sed -i '$ s/$/ rfkill.default_state=1/' /boot/cmdline.txt
sed -i '$ s/$/ overlayroot=tmpfs:recurse=0/' /boot/cmdline.txt

{
	echo "dtparam=i2c_vc=on"
	echo "dtparam=i2c_vc_baudrate=400000"
	echo "dtparam=i2c_arm_baudrate=400000"
	echo "disable_splash=1"
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

# Disable NetworkManager in place of dhcpcd
time apt install "${APT_NONINTERACTIVE}" --fix-broken --fix-missing --no-upgrade \
	dhcpcd5
systemctl disable NetworkManager.service
echo "hostname" >>/etc/dhcpcd.conf
systemctl enable dhcpcd.service
# ln -s /lib/systemd/system/dhcpcd.service /mnt/etc/systemd/system/multi-user.target.wants/dhcpcd.service
{
	echo 'country=US'
	echo 'ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev'
	echo 'update_config=1'
	echo 'network={'
	echo '	ssid="CETI"'
	echo '	psk="Talk2Whales"'
	echo '}'
} >>/etc/wpa_supplicant/wpa_supplicant-wlan0.conf
chmod 600 /etc/wpa_supplicant/wpa_supplicant-wlan0.conf

# Disable periodic systemd services
systemctl disable \
	apt-daily.timer \
	apt-daily-upgrade.timer \
	man-db.timer \
	fstrim.timer \
	dpkg-db-backup.timer \
	cron.service \
	e2scrub_all.timer \
	logrotate.timer

# Disable unneeded services
systemctl disable \
	bluetooth.target \
	ModemManager.service \
	triggerhappy.socket \
	keyboard-setup.service \
	fake-hwclock.service

#disable uart console
sed -i 's/\(.*\)console=serial0,115200 \(.*\)/\1\2/' /boot/cmdline.txt

#disable hdmi
sed -i 's/hdmi_blanking=.*/hdmi_blanking=2/' /boot/config.txt || echo "hdmi_blanking=2" >>/boot/config.txt
sed -i 's/hdmi_force_hotplug=.*/hdmi_force_hotplug=0/' /boot/config.txt || echo "hdmi_force_hotplug=0" >>/boot/config.txt

# Add useful commands to the bash history.
rm -f /home/pi/.bash_history
dos2unix /usr/lib/raspberrypi-sys-mods/custom_bash_history.txt
mv /usr/lib/raspberrypi-sys-mods/custom_bash_history.txt /home/pi/.bash_history

# Sourceforge install of stm32flash to get 0.7
git clone https://git.code.sf.net/p/stm32flash/code stm32flash-code
make install -C stm32flash-code -j4
rm -rf stm32flash-code

# move location of syslogs to volatile partition to ensure logging is captured
# forward journalctl to rsyslog
sed -i 's,var/log/\(.[a-zA-Z]*\)\(\.log\)\?,data/\1.log,g' /etc/rsyslog.conf
sed -i 's/#\(ForwardToSyslog\|MaxLevelSyslog\)/\1/g' /etc/systemd/journald.conf

# add package directories to user PATH for bash to autofill names
# shellcheck disable=SC2016
echo 'export PATH="$PATH:/opt/ceti-tag-data-capture/bin:/opt/ceti-tag-data-capture/ipc"' >>/home/pi/.bashrc
# add package directories to sudoers' secure path so executables and scripts are executable
# shellcheck disable=SC2016
sed -i 's,secure_path="\(.*\)",secure_path="\1:/opt/ceti-tag-data-capture/bin:/opt/ceti-tag-data-capture/ipc",g' /etc/sudoers
