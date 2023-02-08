//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef BURNWIRE_H
#define BURNWIRE_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "utils/logging.h"
#include <pigpio.h>

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------
#define ADDR_MAINTAG_IOX 0x38 // NOTE also defined in recovery.h
// IO Expander
#define BW_nON 0x10			//Burnwire controls
#define BW_RST 0x20

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_burnwire();
int burnwireOn(void);
int burnwireOff(void);

#endif // BURNWIRE_H







