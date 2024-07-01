//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "recovery.h"
#include "iox.h"

#include <unistd.h> //for usleep()

/**
 * @brief Initializes pi hardware to be able to control the recovery board
 * 
 * @return WTResult 
 */
WTResult wt_recovery_init(void){
    //initialize iox pins
    WT_TRY(wt_iox_init());

    //set 3v3_RF enable, keep recovery off
    WT_TRY(wt_iox_set_mode(WT_IOX_GPIO_3V3_RF_EN, WT_IOX_MODE_OUTPUT));
    WT_TRY(wt_recovery_off());

    //set boot0 pin output and pull high
    WT_TRY(wt_iox_set_mode(WT_IOX_GPIO_BOOT0, WT_IOX_MODE_OUTPUT));
    WT_TRY(wt_iox_write(WT_IOX_GPIO_BOOT0, 1));

    return WT_OK;
}

/**
 * @brief Cuts power to recovery board
 * 
 * @return WTResult 
 */
WTResult wt_recovery_off(void){ 
    return wt_iox_write(WT_IOX_GPIO_3V3_RF_EN, 0);
}

/**
 * @brief Applies power to recovery board
 * 
 * @return WTResult 
 */
WTResult wt_recovery_on(void){ 
    return wt_iox_write(WT_IOX_GPIO_3V3_RF_EN, 1);
}

/**
 * @brief Restarts recovery board by toggling power
 * 
 * @return WTResult 
 */
WTResult wt_recovery_restart(void){
   WT_TRY(wt_recovery_off());
   usleep(1000);
   return wt_recovery_on();
}

/**
 * @brief Puts recovery board into bootloader state by pulling boot0 low while
 * restarting the board.
 * 
 * @return WTResult 
 */
WTResult wt_recovery_enter_bootloader(void){
    //pull boot0 low
    WT_TRY(wt_iox_write(WT_IOX_GPIO_BOOT0, 0));

    //toggle power
    WT_TRY(wt_recovery_restart());

    //let system boot in boot loader mode
    usleep(1000);
    return wt_iox_write(WT_IOX_GPIO_BOOT0, 1);
}
