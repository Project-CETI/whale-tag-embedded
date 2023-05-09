# CETI Embedded Software for Tag V3.0
## Background
This repository contains the source code for the software
that is used to build the image for the embedded computer
inside the Whale Tags to be deployed onto the sperm whales
in the ocean during data collection for [Project CETI](https://www.projectceti.org/).

This repository is specifically focusing on a rewrite of the previous code for a new hardware stack up. 
The alternate repository is for a Raspberry Pi Zero W target, while this targets the STM32U575ZI-Q Nucleo
Development board. 

## Setup
Development was done using the [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) 
and the [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html). There are a multitude
of other [STM development tools](https://www.st.com/en/development-tools.html) that are available for use as well,
but these are the only two currently in use for this project. 

Follow the instructions on the STM site to install both the development tools to a location of your choosing. 
Clone this repository directly to location of your choosing:
```
git clone https://github.com/Project-CETI/whale-tag-embedded-2.git
```

Open the STM32CubeIDE and navigate to the project import popup by going to File->Open Projects From Filesystem. 
The popup will give you the ability to select an import source. Click on "Directory..." and navigate to where the repository was cloned. Once located, click on "Open" and then "Finish" to complete the import. 

There are no additional respositories or libraries needed for development at this stage.

## Building, Debugging and Deploying
When wanting to build the project, simply click the hammer icon in the top left hand side of the top bar. Alternatively, go to Project->Build All to build the project. 

To access and run the debugger, click the bug icon closer to the middle portion of the top bar. Alternatively, go to Run->Debug.

To deploy and run the code, click the play button icon next to the debug icon on the top bar. Alternatively, go to Run->Run. 