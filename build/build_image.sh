#!/bin/bash
set -ex

if [[ "$#" -ne 1 ]]; then
  echo "$0 <out_dir>" >&2
  exit 1
fi

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly OUT_DIR="$1"
readonly IMAGE="$OUT_DIR/sdcard.img"

function shell_image {
  sudo PYTHONDONTWRITEBYTECODE=yes "${SCRIPT_DIR}/shell_image.py" "$@"
}

function expand_image {
  sudo PYTHONDONTWRITEBYTECODE=yes "${SCRIPT_DIR}/expand_image.py" "$@"
}

# Resize image if needed.
if ! shell_image "${IMAGE}" "ls /tmp/resized"; then
  expand_image --expand-bytes $((500*1024*1024)) "${IMAGE}"
  shell_image "${IMAGE}" "touch /tmp/resized"
fi

# Build debian packages except
"${SCRIPT_DIR}/../packages/make_dpkg.sh" "${OUT_DIR}"

# Run image setup script.
shell_image \
    --mount "${OUT_DIR}:/out" \
    --mount "${SCRIPT_DIR}/../overlay:/overlay" \
    --arg /out \
    --arg /overlay \
    "${IMAGE}" < "${SCRIPT_DIR}/setup_image.sh"

# Clean /tmp.
if [[ -n "${CLEAN_TMP}" ]]; then
  shell_image "${IMAGE}" "rm -f /tmp/*"
fi
