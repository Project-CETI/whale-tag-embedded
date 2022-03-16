# whale-tag-embedded

This repository contains the source code for the software
that is used to build the image for the embedded computer
inside the Whale Tags to be deployed onto the sperm whales
in the ocean during data collection for [project CETI](https://www.projectceti.org/).


## hardware, branches && git tags

main branch always points to the development targeting the latest hardware.
If you are looking for artifacts for a specific version of the hardware, look under the releases tab.
Each release for each hardware version is marked with a git tag.
The branch name v0 targets the v0 hardware, which  is a rapsberry pi zero w with octoboard soundcard.
The branch v2 targets the v2 MVP hardware designed by Matt Cummings, with the deploy target of Jan'22.
V2 hardware is a raspberry pi zero w with three custome bonnets. The hydrophones are driven by an fpga.


## building

Linux system is assumed to build the software.

Before you start building, make sure you install prerequisites

```bash
  sudo apt-get remove docker docker-engine docker.io containerd runc
  curl -fsSL https://get.docker.com -o get-docker.sh
  sudo sh get-docker.sh
  sudo apt install dos2unix binfmt-support qemu-system-common qemu-user-static
```

The Whale Tag specific software is wrapped in Debian packages
to simplify management and deployment. To build the current
debian packages and the full filesystem run

```bash
  make build
```

This will create out/ folder and place the generated .deb files
for all the CETI software packages.

The build process will run within a docker container.
This build will start by downloading the lastest raspbian lite image,
then mounting it and running inside QEMU, then natively running commands
inside the setup_image.sh, including the debian packages build.
It will also install all the packages that are built into the resulting sdcard image.


## installing

After you follow the steps to build, the /out folder will contain
all the debian packages built, as well as the sdcard.img.

Then you can use Etcher, or simply
```bash
dd if=out/sdcard.img of=/dev/sdX bs=4M
```

If you decide to only update the data collection software,
you can copy the .deb over and install.
```bash
scp ceti-tag-data-capture_X.X-X_all.deb pi:raspberrypi:~
ssh pi@raspberrypi
dpkg -i ceti-tag-data-capture_X.X-X_all.deb
```


## packages

As you could see from above, the custom code for the whale tags is wrapped
in a set of debian packages. Below is the list of all of them with the description of what they do.


## ceti-tag-set-hostname

The source code is in packages/ceti-tag-set-hostname.
The main script that is executed by the systemd is /opt/ceti-tag-set-hostname/ceti-tag-set-hostname.sh

This script is run at device start, and changes the hostname to unique identifiable name. The name is derived from "wt-" and MAC address of the Wi-Fi adapter, which should be unique. If no Wi-Fi adapter is found in the system, an ethernet MAC address is used, if Ethernet is absent as well,
/proc/cpuinfo serial number is used.

The reason the system hostname is changed are two-fold:
1) to avoid hostname collisions if two or more tags are on the same network
2) for the data ingestion pipeline to be able to uniquely identify the source of data, and collate the ingested data from different sessions

For more info on data ingestion see [this doc](https://docs.google.com/document/d/181EHvxuhCzK52iVt1-lNrv1JLxsavYCylSLfFJ6ssQ0/edit#).



## ceti-tag-data-capture

The source code is in packages/ceti-tag-data-capture.
The main script that is executed by the systemd is
/opt/ceti-tag-data-capture/ceti-tag-data-capture.sh

This is the main data collection python script. It spins up several processes in parallel to collect the audio data, and IMU data. All of the files are assumed to go to /data folder.

Note this script is started on system startup and is being restarted if things go wrong in attempt to provide the most reliability.


## updating debian package versions

All debian packages have versions, the ones here are no exception.
The idea is that each update of the software that requires to reinstall the packages, would have the version updated as well. To do so, one needs
to update the debian/changelog in the folder of the package being updated.
Copy-paste the existing description on top, update the version, add description, rebuild the package. After that you can go ahead and reinstall the .deb as described in "#installing"
