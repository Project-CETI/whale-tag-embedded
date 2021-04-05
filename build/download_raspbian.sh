#!/bin/bash
set -ex

if [ "$#" -ne 1 ]; then
    echo "$0 <out_dir>" >&2
    exit 1
fi

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly RASPBIAN_DIR="$(realpath "${SCRIPT_DIR}/../raspbian")"
readonly RASPBIAN_ZIP="${RASPBIAN_DIR}/raspbian.zip"
readonly RASPBIAN_IMG="${RASPBIAN_DIR}/raspbian.img"
readonly RASPBIAN_URL="https://downloads.raspberrypi.org/raspbian_lite_latest"
readonly OUT_DIR="$(realpath "$1")"

mkdir -p "${RASPBIAN_DIR}"

if [ ! -f "${RASPBIAN_IMG}" ]; then
    rm -rf "${RASPBIAN_DIR:-.}/*"
    wget -O "${RASPBIAN_ZIP}" "${RASPBIAN_URL}"
    unzip "${RASPBIAN_ZIP}"
    mv *rasp*.img "${RASPBIAN_IMG}"
fi
cp -f "${RASPBIAN_IMG}" "${OUT_DIR}/sdcard.img"
