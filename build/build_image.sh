#!/bin/bash
set -ex

if [[ "$#" -ne 1 ]]; then
  echo "$0 <out_dir>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$1"
IMAGE="$OUT_DIR/sdcard.img"

function shell_image {
  sudo PYTHONDONTWRITEBYTECODE=yes "${SCRIPT_DIR}/shell_image.py" "$@"
}

function expand_image {
  sudo PYTHONDONTWRITEBYTECODE=yes "${SCRIPT_DIR}/expand_image.py" "$@"
}

function add_data_partition {
  sudo PYTHONDONTWRITEBYTECODE=yes "${SCRIPT_DIR}/add_partition.py" "$@"
}

# Resize image if needed.
if ! shell_image "${IMAGE}" "ls /tmp/resized"; then
  expand_image --expand-bytes $((500*1024*1024)) "${IMAGE}"
  add_data_partition --expand-bytes $((500*1024*1024)) "${IMAGE}"
  shell_image "${IMAGE}" "touch /tmp/resized"
fi

# Run image setup script.
shell_image \
    --mount "${OUT_DIR}:/out" \
    --mount "${SCRIPT_DIR}:/build" \
    --mount "${SCRIPT_DIR}/../overlay:/overlay" \
    --mount "${SCRIPT_DIR}/../packages:/packages" \
    --arg /out \
    --arg /build \
    --arg /overlay \
    --arg /packages \
    "${IMAGE}" < "${SCRIPT_DIR}/setup_image.sh"

# Clean /tmp.
if [[ -n "${CLEAN_TMP}" ]]; then
  shell_image "${IMAGE}" "rm -f /tmp/*"
fi
