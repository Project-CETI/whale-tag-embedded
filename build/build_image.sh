#!/bin/bash
set -ex

if [[ "$#" -ne 1 ]]; then
  echo "$0 <out_dir>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$1"
IMAGE="$OUT_DIR/sdcard.img"

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
