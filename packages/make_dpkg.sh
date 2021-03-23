#!/bin/bash
set -ex

if [[ "$#" -ne 1 ]]; then
  echo "$0 <deb_dir>" >&2
  exit 1
fi

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly DEB_DIR="$(cd "$1" && pwd)"
readonly PACKAGES=( \
    "${SCRIPT_DIR}/ceti-tag-set-hostname" \
)

function build_package {
  pushd "$1"
  local package=$(cat debian/control | grep Package | awk '{print $2}')
  dpkg-buildpackage -b -rfakeroot -us -uc -tc
  mv ../${package}*.deb ../${package}*.buildinfo ../${package}*.changes "${DEB_DIR}"
  popd
}

for package in "${PACKAGES[@]}"; do
  if [[ -n "${AIY_BUILD_PARALLEL}" ]]; then
    build_package "${package}" &
  else
    build_package "${package}"
  fi
done

wait
