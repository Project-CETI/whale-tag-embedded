#!/bin/bash
set -ex

if [ "$#" -ne 2 ]; then
    echo "$0 <out_dir> <overlay-dir>" >&2
    exit 1
fi

OUT_DIR="$1"
OVERLAY_DIR="$2"

APT_NONINTERACTIVE="-y"
export DEBIAN_FRONTEND="noninteractive"
export APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=yes

function install_package {
  deb_file="$1"
  package=$(dpkg-deb -f "${deb_file}" Package)
  sha256=$(sha256sum "${deb_file}" | awk '{print $1}')
  sha256_file="/tmp/${package}.sha256"

  if ! dpkg -l "${package}" ; then
    echo "Package ${package} is not installed."
    dpkg -i "${deb_file}"
    echo "${sha256}" > "${sha256_file}"
  else
    echo "Package ${package} is already installed."
    installed_sha256="$(cat "${sha256_file}" 2>/dev/null || echo '')"
    if [[ "${installed_sha256}" != "${sha256}" ]]; then
      echo "Checksums do not ${package} match."
      dpkg --purge --force-all "${package}"
      dpkg -i "${deb_file}"
      echo "${sha256}" > "${sha256_file}"
    else
      echo "Checksums for ${package} match."
    fi
  fi
}


# Setup aiyprojects apt repo.
echo "deb https://packages.cloud.google.com/apt aiyprojects-stable main" > /etc/apt/sources.list.d/aiyprojects.list
if ! apt-key list | grep "Google Cloud"; then
  curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | apt-key add -
fi

apt update

time apt install "${APT_NONINTERACTIVE}" --fix-broken --no-upgrade \
  alsa-utils \
  aiy-usb-gadget \
  avahi-utils \
  bc \
  dkms \
  dnsmasq \
  flac \
  libi2c-dev \
  i2c-tools \
  python3 \
  python3-gpiozero \
  python3-smbus \
  python3-pip \
  python3-bluez \
  python3-dbus \
  netcat \


apt "${APT_NONINTERACTIVE}" autoremove

pip3 install --retries 10 --default-timeout=60 \
                  --no-deps --no-cache-dir --disable-pip-version-check \
                  -r /dev/stdin <<EOF
cachetools==4.1.1
sparkfun-qwiic-icm20948
adafruit-circuitpython-bno08x
adafruit-circuitpython-ads1x15
EOF

# Enable I2C
sudo raspi-config nonint do_i2c 0

# Enable SSH.
touch /boot/ssh

# Update keyboard layout.
sed -i -e 's/"gb"/"us"/' /etc/default/keyboard

# Change default password
echo -e "ceticeti\nceticeti" | passwd pi

# Create /data folder - that's where the captured data will go
mkdir -p /data
chmod 777 /data

# Install PiSugar
TEMP_DEB="$(mktemp)".deb
wget -O "$TEMP_DEB" 'https://github.com/PiSugar/pisugar-power-manager-rs/releases/download/v1.4.9/pisugar-server_1.4.9_armhf.deb'
dpkg -i "$TEMP_DEB"
rm -f "$TEMP_DEB"

TEMP_DEB="$(mktemp)".deb
wget -O "$TEMP_DEB" 'https://github.com/PiSugar/pisugar-power-manager-rs/releases/download/v1.4.9/pisugar-poweroff_1.4.9_armhf.deb'
dpkg -i "$TEMP_DEB"
rm -f "$TEMP_DEB"

# Install octoboard audio injector
apt remove pulseaudio
TEMP_DEB="$(mktemp)".deb
wget -O "$TEMP_DEB" 'https://github.com/Audio-Injector/stereo-and-zero/raw/master/audio.injector.scripts_0.1-1_all.deb'
dpkg -i "$TEMP_DEB"
rm -f "$TEMP_DEB"
audioInjector-setup.sh

# Install whale tag packages.
install_package "$(ls "${OUT_DIR}"/ceti-tag-set-hostname_*_all.deb)"
install_package "$(ls "${OUT_DIR}"/ceti-tag-data-capture_*_all.deb)"
install_package "$(ls "${OUT_DIR}"/ceti-tag-burnwire-shutdown_*_all.deb)"

# Minimize logging
# See article: https://medium.com/swlh/make-your-raspberry-pi-file-system-read-only-raspbian-buster-c558694de79
apt remove --purge -y triggerhappy logrotate dphys-swapfile
apt autoremove --purge -y
apt install -y busybox-syslogd
apt remove --purge -y rsyslog
rm -rf /var/lib/dhcp /var/lib/dhcpcd5 /var/spool /etc/resolv.conf
ln -s /tmp /var/lib/dhcp
ln -s /tmp /var/lib/dhcpcd5
ln -s /tmp /var/spool
touch /tmp/dhcpcd.resolv.conf
ln -s /tmp/dhcpcd.resolv.conf /etc/resolv.conf

################################################################################
#################################### pi user ###################################
################################################################################

# Copy filesystem overlay.
tar -cf - -C "${OVERLAY_DIR}" --owner=pi --group=pi . | tar -xf - -C /

# All done
echo "( ・◡・)つ━☆   Build complete"
