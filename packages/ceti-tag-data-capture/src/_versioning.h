//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
//
// Version    Date    Description
//  2_1.0   10/08/21   Begin work, establish framework
//  2_1.1   06/27/22   Update v2.0 to work with v2.1 hardware
//  2_1.2   07/03/22   Fix deploy timeout bug
//  2_1.3   08/17/22   Bit-banged i2c for IMU, serial comms for Recovery Board
//  2_1.4   11/3/22    Add battery check funcs, general clean up. FPGA shutdown method
//  2_1.5   11/24/22   Change RAM buffer size to accomodate read-only rootfs
//  2.1.6   12/03/22   Added ECG
//  2.2.0   03/25/22   Refactored codebase, testing on Pi Zero 2
//  2.2.1   11/8/23    Adding battery temperature monitoring
//  2.3.0   05/31/24   Changes for V2_3 hardware, new branch, preliminary work

#ifndef CETI_VERSIONING_H
#define CETI_VERSIONING_H

#define CETI_VERSION "v2_3.0 New hardware design - V2.3"

#endif // CETI_VERSIONING_H

/*------------------------------------------------------------------------------

2.3.0 	This version is specific to new hardware and is not backward compatible

//----------------------------------------------------------------------------*/