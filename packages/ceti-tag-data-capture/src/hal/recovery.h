//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//               [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_RECOVERY__
#define __CETI_WHALE_TAG_HAL_RECOVERY__

#include "error.h" //for WTResult

WTResult wt_recovery_init(void);
WTResult wt_recovery_off(void);
WTResult wt_recovery_on(void);
WTResult wt_recovery_restart(void);
WTResult wt_recovery_enter_bootloader(void);

#endif //__CETI_WHALE_TAG_HAL_RECOVERY__
