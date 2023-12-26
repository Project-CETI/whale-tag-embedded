#!/bin/bash
set -e

if [ "$#" -ne 4 ]; then
    echo "$0 <out_dir> <build-dir> <overlay-dir> <packages-dir>" >&2
    exit 1
fi

OUT_DIR="$1"
OVERLAY_DIR="$3"

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
  zlib1g-dev \

apt "${APT_NONINTERACTIVE}" autoremove

# get current date
date -s "$(curl -s --head http://google.com | grep ^Date: | sed 's/Date: //g')"

# Isolate CPUs that will be used for audio and ECG capture,
#  so kernel/system processes are scheduled on other cores.
sed -i '$ s/$/ isolcpus=2,3/' /boot/cmdline.txt


# Setup hardware parameters for i2c
raspi-config nonint do_i2c 0
{
  echo "dtparam=i2c_vc=on"
  echo "dtparam=i2c_vc_baudrate=400000"
  echo "dtparam=i2c_arm_baudrate=400000"
} >> /boot/config.txt

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
echo "/dev/disk/by-label/cetiData /data ext4 defaults,nofail 0 0" >> /etc/fstab

# Build and install whale tag software as debian packages
/packages/make_dpkg.sh "${OUT_DIR}"
install_package "$(ls "${OUT_DIR}"/ceti-tag-data-capture_*.deb)"

# Do not run pi wizard
rm -rf /etc/xdg/autostart/piwiz.desktop

# Disable first boot rootfs resize
rm /etc/init.d/resize2fs_once

# Copy filesystem overlay.
tar -cf - -C "${OVERLAY_DIR}" --owner=pi --group=pi . | tar -xf - -C /

# Add useful commands to the bash history.
rm -f /home/pi/.bash_history
dos2unix /usr/lib/raspberrypi-sys-mods/custom_bash_history.txt
mv /usr/lib/raspberrypi-sys-mods/custom_bash_history.txt /home/pi/.bash_history

# All done
echo "( ・◡・)つ━☆   Build complete"
