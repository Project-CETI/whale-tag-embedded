#!/bin/bash
set -e

if [ "$#" -ne 1 ]; then
    echo "$0 <out_dir>" >&2
    exit 1
fi

OUT_DIR="$1"

function install_package {
  deb_file="$1"
  package=$(dpkg-deb -f "${deb_file}" Package)
  sha256=$(sha256sum "${deb_file}" | awk '{print $1}')
  sha256_file="/tmp/${package}.sha256"

  if ! dpkg -l "${package}" ; then
    echo "Package ${package} is not installed."
    dpkg -i "${deb_file}"
    echo "${sha256}" > "${sha256_file}"
  else
    echo "Package ${package} is already installed."
    installed_sha256="$(cat "${sha256_file}" 2>/dev/null || echo '')"
    if [[ "${installed_sha256}" != "${sha256}" ]]; then
      echo "Checksums do not ${package} match."
      dpkg --purge --force-all "${package}"
      dpkg -i "${deb_file}"
      echo "${sha256}" > "${sha256_file}"
    else
      echo "Checksums for ${package} match."
    fi
  fi
}

# Build and install whale tag software as debian packages
install_package "$(ls "${OUT_DIR}"/ceti-tag-data-capture_*.deb)"
