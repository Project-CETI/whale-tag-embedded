#!/bin/bash
set -e

if [ "$#" -ne 1 ]; then
    echo "$0 <out_dir>" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RASPIOS_DIR="$(realpath "${SCRIPT_DIR}/../raspios")"
RASPIOS_IMG="${RASPIOS_DIR}/raspios.img"
OUT_DIR="$(realpath "$1")"
IMAGE="${OUT_DIR}/sdcard.img"

mkdir -p "${RASPIOS_DIR}"
if [ ! -f "${RASPIOS_IMG}" ]; then
    rm -rf "${RASPIOS_DIR}/*"
    echo "Downloading the latest raspios..."
    "${SCRIPT_DIR}"/rpi-image download --suffix raspios-bullseye-arm64-lite --output "${RASPIOS_IMG}"
fi
cp -f "${RASPIOS_IMG}" "${IMAGE}"

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
