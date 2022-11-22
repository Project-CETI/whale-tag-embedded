#!/bin/bash
set -ex

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
  expand_image "${IMAGE}"
  add_data_partition "${IMAGE}"
  shell_image "${IMAGE}" "touch /tmp/resized"
fi

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

# Clean /tmp.
if [[ -n "${CLEAN_TMP}" ]]; then
  shell_image "${IMAGE}" "rm -f /tmp/*"
fi
