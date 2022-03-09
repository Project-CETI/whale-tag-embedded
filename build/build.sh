#!/bin/bash
set -ex

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$1"

read -r -d '' SCRIPT << EOF || true
groupadd --gid $(id -g) $(id -g -n);
useradd -m -e "" -s /bin/bash --gid $(id -g) --uid $(id -u) $(id -u -n);
passwd -d $(id -u -n);
echo "$(id -u -n) ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers;
sudo -E -u $(id -u -n) /build/build_image.sh /out;
EOF

docker run --rm --privileged -i "$(tty -s && echo --tty)" \
  --volume "${SCRIPT_DIR}":/build \
  --volume "${SCRIPT_DIR}/../overlay":/overlay \
  --volume "${OUT_DIR}":/out \
  --volume "${SCRIPT_DIR}/../packages":/packages \
  sdcard-builder \
  /bin/bash -c "${SCRIPT}"
