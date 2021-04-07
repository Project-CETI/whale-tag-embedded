# whale-tag-embedded

This repo contains the source code for the software
that is used to build the image for the embedded computer
inside the Whale Tags to be deployed onto the sperm whales
in the ocean during data collection for [project CETI](https://www.projectceti.org/).


# building

Linux system is assumed to build the software.

The Whale Tag specific software is wrapped in Debian packages
to simplify management and deployment. To build the current 
debian packages run

```
  make build-debs
```

This will generate .deb files for all sources in packages.
The .deb files will be located in out/ folder.

If you want to create the full sdcard image you will need docker
installed on your system. You will also need make. 


Then just run make:

```
  make build-sdcard-img
```

The build process will run within a docker container. 
This build will start by downloading the lastest raspbian lite image, 
then mounting it and running inside QEMU, then natively running commands
inside the setup_image.sh. It will also install all the packages that 
are built with the make build-debs command.


# installing 

After you follow the steps to build, the /out folder will contain
all the debian packages built, as well as the sdcard.img.

Then you can use Etcher, or simply 
```
dd if=out/sdcard.img of=/dev/sdX bs=4M
```

If you decide to only update the data collection software, 
you can copy the .deb over and install. 
```
scp ceti-tag-data-capture_X.X-X_all.deb pi:raspberrypi:~
ssh pi@raspberrypi
dpkg -i ceti-tag-data-capture_X.X-X_all.deb
```


# packages

As you could see from above, the custom code for the whale tags is wrapped
in a set of debian packages. Below is the list of all of them with the description of what they do.


## ceti-tag-set-hostname

The source code is in packages/ceti-tag-set-hostname.
The main script that is executed by the systemd is /opt/ceti-tag-set-hostname/ceti-tag-set-hostname.sh

This script is run at device start, and changes the hostname to unique identifiable name. The name is derived from "wt-" and MAC address of the WiFi adapter, which should be unique. If no wifi adapter is found in the system, an ethernet MAC address is used, if Ethernet is absent as well, 
/proc/cpuinfo serial number is used.

The reason the system hostname is changed are two-fold:
1) to avoid hostname collisions if two or more tags are on the same network
2) for the data ingestion pipeline to be able to uniquely identify the source of data, and collate the ingested data from different sessions

For more info on data ingestion see [this doc](https://docs.google.com/document/d/181EHvxuhCzK52iVt1-lNrv1JLxsavYCylSLfFJ6ssQ0/edit#).


## ceti-tag-burnwire-shutdown

The source code is in packages/ceti-tag-burnwire-shutdown.
The main script that is executed by the systemd is
/opt/ceti-tag-burnwire-shutdown/ceti-tag-burnwire-shutdown.sh

This is a systemd service that monitors a few conditions and triggers
the burnwire to detach the whale tag from the back of the whale and proceed to safely shutdown the system preserving the data collected.

By default the burnwire is assumed to be attached to GPIO15, and 
enabling it means driving the GPIO15 from LOW to HIGH for 20 seconds.

The conditions that trigger burnwire and shutdown are:

1) Battery level, as read from SigarPi2 UPS.
The level itself is set by the SHUTDOWN_BATTERY_LEVEL="5.0" variable in
opt/ceti-tag-burnwire-shutdown/ceti-tag-burnwire-shutdown.sh

2) Uptime. By default it is turned off. But in case one wants to introduce a limit on for how long the system should be running and collecting data, it is possible to change the variable MAXUPTIME_S="-1"
in the opt/ceti-tag-burnwire-shutdown/ceti-tag-burnwire-shutdown.sh script. Please note, this is the value of the uptime in seconds. So to have the system shutdown after 10 minutes of operations, set this value to "600"


## ceti-tag-data-capture

The source code is in packages/ceti-tag-data-capture.
The main script that is executed by the systemd is
/opt/ceti-tag-data-capture/ceti-tag-data-capture.sh

This is the main data collection python script. It spins up several processes in parallel to collect the audio data, and IMU data. All of the files are assumed to go to /data folder.

Note this script is started on system startup and is being restarted if things go wrong in attempt to provide the most reliability.


# updating debian package versions

All debian packages have versions, the ones here are no exception.
The idea is that each update of the software that requires to reinstall the packages, would have the version updated as well. To do so, one needs
to update the debian/changelog in the folder of the package being updated.
Copy-paste the existing description on top, update the version, add description, rebuild the package. After that you can go ahead and reinstall the .deb as described in "#installing"