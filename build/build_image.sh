#!/bin/bash
set -e

if [[ "$#" -ne 1 ]]; then
  echo "$0 <out_dir>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$1"
IMAGE="$OUT_DIR/sdcard.img"

function shell_image {
  sudo "${SCRIPT_DIR}"/rpi-image run --image "${IMAGE}" "$@"
}

function expand_image {
  sudo "${SCRIPT_DIR}/rpi-image" expand --size +512M --image "${IMAGE}"
}

function add_data_partition {
  sudo "${SCRIPT_DIR}/rpi-image" append --size 128M --filesystem ext4 --label cetiData --image "${IMAGE}"
}

# Resize image if needed.
if ! shell_image ls /tmp/resized; then
  expand_image
  add_data_partition
  shell_image touch /tmp/resized
fi

# Run image setup script.
shell_image \
    --bind "${OUT_DIR}:/out" \
    --bind "${SCRIPT_DIR}:/build" \
    --bind "${SCRIPT_DIR}/../overlay:/overlay" \
    --bind "${SCRIPT_DIR}/../packages:/packages" \
    --bind-ro "${SCRIPT_DIR}/setup_image.sh:/setup_image.sh" \
    "/setup_image.sh" \
    "/out" \
    "/build" \
    "/overlay" \
    "/packages"


# Clean /tmp.
if [[ -n "${CLEAN_TMP}" ]]; then
  shell_image "${IMAGE}" "rm -f /tmp/*"
fi
