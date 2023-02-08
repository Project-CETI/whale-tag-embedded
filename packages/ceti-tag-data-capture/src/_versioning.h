//-----------------------------------------------------------------------------
// CETI Tag Electronics
// Cummings Electronics Labs, October 2021
// Developed under contract for Harvard University Wood Lab
//-----------------------------------------------------------------------------
//
// Note - version must correspond with the repository release tag
//
// Version    Date    Description
//  2_1.0   10/08/21   Begin work, establish framework
//  2_1.1   06/27/22   Update v2.0 to work with v2.1 hardware
//  2_1.2   07/03/22   Fix deploy timeout bug
//  2_1.3   08/17/22   Bit-banged i2c for IMU, serial comms for Recovery Board
//  2_1.4   11/3/22    Add battery check funcs, general clean up. FPGA shutdown method
//  2_1.5   11/24/22   Change RAM buffer size to accomodate read-only rootfs
//  2.1.6   12/03/22   Added ECG
//
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// File: cetiTagWrapper.h
// Description: The Ceti Tag application top level header file
//-----------------------------------------------------------------------------

#ifndef CETI_VERSIONING_H
#define CETI_VERSIONING_H

#define CETI_VERSION "v2_1.6 release candidate - in test"

#endif // CETI_VERSIONING_H