#!/bin/bash
set -ex

if [ "$#" -ne 1 ]; then
    echo "$0 <out_dir>" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RASPIOS_DIR="$(realpath "${SCRIPT_DIR}/../raspios")"
RASPIOS_XZ="${RASPIOS_DIR}/raspios.xz"
RASPIOS_IMG="${RASPIOS_DIR}/raspios.img"
RASPIOS_URL="https://downloads.raspberrypi.org/raspios_lite_arm64/images/raspios_lite_arm64-2022-09-07/2022-09-06-raspios-bullseye-arm64-lite.img.xz"
OUT_DIR="$(realpath "$1")"

mkdir -p "${RASPIOS_DIR}"

if [ ! -f "${RASPIOS_IMG}" ]; then
    rm -rf "${RASPIOS_DIR}/*"
    wget -O "${RASPIOS_XZ}" "${RASPIOS_URL}"
    xz -d -v "${RASPIOS_XZ}"
    mv "${RASPIOS_DIR}/raspios" "${RASPIOS_IMG}"
fi
cp -f "${RASPIOS_IMG}" "${OUT_DIR}/sdcard.img"