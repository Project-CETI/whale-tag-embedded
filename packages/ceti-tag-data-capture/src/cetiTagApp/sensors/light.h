//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#ifndef LIGHT_H
#define LIGHT_H

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Definitions/Configuration
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
int init_light();
void *light_thread(void *paramPtr);
int light_verify(void);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern int g_light_thread_is_running;

#endif // LIGHT_H
