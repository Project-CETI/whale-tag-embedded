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

This will generate .deb files for all packages in out/debs folder