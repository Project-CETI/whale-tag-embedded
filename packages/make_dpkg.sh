#!/bin/bash
set -ex

if [[ "$#" -ne 1 ]]; then
  echo "$0 <deb_dir>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEB_DIR="$(cd "$1" && pwd)"
PACKAGES=( \
    "${SCRIPT_DIR}/ceti-tag-data-capture" \
)

function build_package {
  pushd "$1"
  package="$(< debian/control grep Package | awk '{print $2}')"
  dpkg-buildpackage -b -rfakeroot -us -uc -tc
  mv "../${package}"*.deb "${DEB_DIR}"
  mv "../${package}"*.buildinfo "${DEB_DIR}"
  mv "../${package}"*.changes "${DEB_DIR}"
  popd
}

for deb in "${PACKAGES[@]}"; do
  build_package "${deb}"
done

wait
