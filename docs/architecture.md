# Whale Tag Embedded System Architecture

## Overview

This repository contains the embedded software stack for Project CETI's whale tag devices - sophisticated data collection tags that are deployed on sperm whales to capture acoustic and environmental data for marine biology research.

The system is built around a Raspberry Pi Zero 2 W running a custom Raspbian-based Linux distribution, with specialized hardware for high-quality hydrophone recording and sensor data collection.

## System Architecture

### Hardware Platforms

The software supports multiple hardware generations:

- **v0**: Raspberry Pi Zero W + OctoBoard soundcard
- **v2**: Raspberry Pi Zero W + three custom bonnets + FPGA-driven hydrophones  
- **v2_2**: Raspberry Pi Zero 2 W + enhanced electrical/mechanical design (current production)

### Core Components

#### 1. Main Data Collection Application (`cetiTagApp`)

**Location**: `packages/ceti-tag-data-capture/src/cetiTagApp/`

The primary application responsible for:
- **Audio Acquisition**: High-quality hydrophone recording via I²S interface
- **Sensor Monitoring**: Environmental sensors (pressure, temperature, IMU)
- **Mission State Management**: Coordinated state machine for deployment lifecycle
- **Data Storage**: Efficient file management and data organization
- **Power Management**: Battery monitoring and power optimization
- **Recovery Operations**: GPS tracking and communication during surface intervals

**Key Modules**:
- `state_machine.c` - Core mission state coordination
- `launcher.c` - Application initialization and main loop
- `acq/` - Audio acquisition subsystem
- `sensors/` - Environmental sensor interfaces  
- `recovery.c` - Surface recovery and communication
- `battery.c` - Power management and monitoring

#### 2. Hardware Test System (`cetiHWTest`)

**Location**: `packages/ceti-tag-data-capture/src/cetiHWTest/`

Interactive test utility for:
- Hardware validation during manufacturing
- Field testing and diagnostics
- Pre-deployment verification
- Post-recovery analysis

**Key Features**:
- Terminal-based user interface (`tui.c`)
- Comprehensive hardware test suite (`tests/`)
- IPC integration with main application for safe resource coordination
- Automated test reporting and logging

### Inter-Process Communication (IPC)

The system uses **named pipes** for reliable inter-process communication:

- **Command Pipe** (`/tmp/cetiCommand`): Send commands to main application
- **Response Pipe** (`/tmp/cetiResponse`): Receive acknowledgments and status

**Supported Commands**:
- `mission pause` - Pause data collection for maintenance/testing
- `mission resume` - Resume normal operation
- `status` - Query current system state
- `shutdown` - Graceful system shutdown

### Build System & Deployment

#### Docker-Based Cross-Compilation

The build system uses Docker containers with QEMU emulation to:
- Cross-compile for ARM architecture on x86 hosts
- Create reproducible build environments
- Generate complete filesystem images
- Package software as Debian packages for easy deployment

**Build Process**:
```bash
make build  # Complete system build
make packages  # Debian packages only
```

#### Package Management

Software is distributed as Debian packages:

- **`ceti-tag-data-capture`**: Main data collection applications
- **`ceti-tag-set-hostname`**: Unique device identification system

### Data Management

#### File Organization
```
/data/
├── audio/          # Hydrophone recordings (WAV format)
├── sensors/        # Environmental sensor logs
├── logs/           # System and application logs
├── config/         # Runtime configuration
└── recovery/       # GPS tracks and recovery data
```

#### Data Ingestion Pipeline
- Unique device identification via MAC address-based hostnames
- Standardized file naming for automated ingestion
- Compressed data transfer during recovery periods
- Integration with cloud-based analysis infrastructure

### Mission State Machine

The core application operates through a structured state machine:

1. **BOOT** - System initialization
2. **MISSION_READY** - Pre-deployment configuration
3. **MISSION_RUNNING** - Active data collection underwater
4. **MISSION_PAUSED** - Temporary suspension (testing, maintenance)
5. **SURFACE_RECOVERY** - GPS tracking and data upload
6. **MISSION_COMPLETE** - End of deployment lifecycle

### Hardware Interfaces

#### Audio Acquisition
- **I²S Interface**: High-bandwidth digital audio from hydrophones
- **Multi-channel**: Support for stereophonic and multi-hydrophone arrays
- **Real-time Processing**: Low-latency audio capture with minimal dropouts

#### Sensor Integration
- **SPI Bus**: High-speed sensor communication
- **I²C Bus**: Low-speed sensor and configuration interfaces
- **GPIO**: Burnwire control, status LEDs, and discrete I/O

#### Power Management
- **Battery Monitoring**: Real-time voltage and current measurement
- **Power Optimization**: Dynamic performance scaling based on battery state
- **Burnwire Control**: Timed release mechanism for tag recovery

### Development & Testing

#### Build Prerequisites
```bash
# Docker installation
sudo apt-get remove docker docker-engine docker.io containerd runc
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# QEMU for cross-compilation
sudo apt install dos2unix binfmt-support qemu-system-common qemu-user-static
```

#### Testing Strategy
- **Hardware-in-the-Loop**: Real hardware testing via `cetiHWTest`
- **Unit Tests**: Component-level validation
- **Integration Tests**: End-to-end system validation
- **Field Testing**: Ocean deployment validation

### Security & Reliability

#### Fault Tolerance
- **Automatic Recovery**: Service restart on application crashes
- **Watchdog Monitoring**: Hardware watchdog timer integration
- **Data Integrity**: Checksums and validation for critical data
- **Power Failure Recovery**: Graceful handling of unexpected power loss

#### Data Security
- **Unique Identification**: Cryptographically-derived device IDs
- **Secure Storage**: Protected configuration and sensitive data
- **Network Security**: Encrypted communications during recovery

## Getting Started

### Building the System
```bash
# Clone repository
git clone <repository-url>
cd whale-tag-embedded

# Build complete system
make build

# Output artifacts in out/ directory:
# - sdcard.img (complete system image)
# - *.deb packages (for updates)
```

### Deployment
```bash
# Flash complete image
dd if=out/sdcard.img of=/dev/sdX bs=4M

# Or update packages only
scp *.deb pi@whale-tag:~
ssh pi@whale-tag "sudo dpkg -i *.deb"
```

### Hardware Testing
```bash
# Run interactive hardware tests
sudo cetiHWTest

# The test system will automatically:
# 1. Pause the main data collection application
# 2. Run comprehensive hardware validation
# 3. Resume normal operation when complete
```

## Contributing

This is an open-source project following standard GitHub workflows:

1. **Fork** the repository
2. **Create feature branch** from appropriate base branch (main, v0, v2, v2_2)
3. **Implement changes** with comprehensive testing
4. **Submit pull request** with detailed description and test results

See existing issues for contribution opportunities, particularly hardware validation and testing improvements.
