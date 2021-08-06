#!/bin/bash
set -ex

if [[ "$#" -ne 1 ]]; then
  echo "$0 <deb_dir>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEB_DIR="$(cd "$1" && pwd)"
PACKAGES=( \
    "${SCRIPT_DIR}/ceti-tag-set-hostname" \
    "${SCRIPT_DIR}/ceti-tag-data-capture" \
    "${SCRIPT_DIR}/ceti-tag-burnwire-shutdown" \
)

function build_package {
  pushd "$1"
  package="$(< debian/control grep Package | awk '{print $2}')"
  dpkg-buildpackage -b -rfakeroot -us -uc -tc
  mv "../${package}*.deb ../${package}*.buildinfo ../${package}*.changes" "${DEB_DIR}"
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