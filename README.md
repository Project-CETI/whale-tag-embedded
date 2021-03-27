# whale-tag-embedded

This repo contains the source code for the software
that is used to build the image for the embedded computer
inside the Whale Tags to be deployed onto the sperm whales
in the ocean during data collection for [project CETI](https://www.projectceti.org/).

# Building

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
/out folder will contain all the debian packages built, as well as
the sdcard.img.

Then you can use Etcher, or simply 
```
dd if=out/sdcard.img of=/dev/sdX bs=4M
```