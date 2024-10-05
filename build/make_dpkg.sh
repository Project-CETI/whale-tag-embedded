#!/bin/bash
set -ex

if [[ "$#" -ne 2 ]]; then
  echo "$0 <deb_dir> <out_dir>" >&2
  exit 1
fi

DEB_DIR=$2

pushd "$1"
package="$(< debian/control grep Package | awk '{print $2}')"
DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -b -rfakeroot -us -uc -tc
mv "../${package}"*.deb "${DEB_DIR}"
mv "../${package}"*.buildinfo "${DEB_DIR}"
mv "../${package}"*.changes "${DEB_DIR}"
popd
