#!/bin/bash
set -ex

if [ "$#" -ne 4 ]; then
    echo "$0 <out_dir> <build-dir> <overlay-dir> <packages-dir>" >&2
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

apt -y update --allow-releaseinfo-change
apt -y upgrade

time apt install "${APT_NONINTERACTIVE}" --fix-broken --no-upgrade \
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
  libi2c-dev \
  i2c-tools \
  netcat \


apt "${APT_NONINTERACTIVE}" autoremove

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


# Build and install whale tag software as debian packages
/packages/make_dpkg.sh "${OUT_DIR}"
install_package "$(ls "${OUT_DIR}"/ceti-tag-set-hostname_*.deb)"
install_package "$(ls "${OUT_DIR}"/ceti-tag-data-capture_*.deb)"

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
