#!/bin/bash
set -ex

if [ "$#" -ne 2 ]; then
    echo "$0 <out_dir> <overlay-dir>" >&2
    exit 1
fi

readonly OUT_DIR="$1"
readonly OVERLAY_DIR="$2"

readonly APT_NONINTERACTIVE="-y -o Dpkg::Options::=--force-confdef -o Dpkg::Options::=--force-confold"
export readonly DEBIAN_FRONTEND="noninteractive"
export readonly APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=yes

function install_package {
  local deb_file="$1"
  local package=$(dpkg-deb -f "${deb_file}" Package)
  local sha256=$(sha256sum "${deb_file}" | awk '{print $1}')
  local sha256_file="/tmp/${package}.sha256"

  if ! dpkg -l "${package}" ; then
    echo "Package ${package} is not installed."
    dpkg -i "${deb_file}"
    echo "${sha256}" > "${sha256_file}"
  else
    echo "Package ${package} is already installed."
    local installed_sha256="$(cat "${sha256_file}" 2>/dev/null || echo '')"
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

time apt install ${APT_NONINTERACTIVE} --fix-broken --no-upgrade \
  alsa-utils \
  avahi-utils \
  aiy-usb-gadget \
  dnsmasq \
  flac \
  i2c-tools \
  python3 \
  python3-gpiozero \
  python3-smbus \
  python3-pip \
  python3-bluez \
  python3-dbus \

apt ${APT_NONINTERACTIVE} autoremove

pip3 install --retries 10 --default-timeout=60 \
                  --no-deps --no-cache-dir --disable-pip-version-check \
                  -r /dev/stdin <<EOF
cachetools==4.1.1
sparkfun-qwiic-icm20948
EOF

# Install general packages.
install_package $(ls ${OUT_DIR}/ceti-tag-set-hostname_*_all.deb)
install_package $(ls ${OUT_DIR}/ceti-tag-data-capture_*_all.deb)

# Enable SSH.
touch /boot/ssh

# Update boot configuration (/boot/config.txt).
sed -i -e "s/^dtparam=audio=on/#\0/" /boot/config.txt
sed -i -e "s/^dtparam=spi=on/#\0/" /boot/config.txt
sed -i -e "s/^dtparam=i2c_arm=on/#\0/" /boot/config.txt

# Update keyboard layout.
sed -i -e 's/"gb"/"us"/' /etc/default/keyboard

################################################################################
#################################### pi user ###################################
################################################################################

# Copy filesystem overlay.
tar -cf - -C "${OVERLAY_DIR}" --owner=pi --group=pi . | tar -xf - -C /

echo "( ・◡・)つ━☆   Build complete"