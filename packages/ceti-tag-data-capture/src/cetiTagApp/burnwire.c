//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include "burnwire.h"
#include "device/fpga.h" // for LED control
#include "device/iox.h"
#include "led_ctrl.h"

#include "utils/logging.h"

static int s_burnwire_led_state = 0; // 0 = red, 1 = yellow, 2 = green
//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
static WTResult __burnwire_on(void) {
    return iox_write_pin(IOX_GPIO_BURNWIRE_ON, 1);
}

static WTResult __burnwire_off(void) {
    return iox_write_pin(IOX_GPIO_BURNWIRE_ON, 0);
}

static WTResult __burnwire_init(void) {
    // Initialize I2C/GPIO functionality.
    WT_TRY(iox_init()); // still initialize incase ecg is not
    WT_TRY(iox_set_mode(IOX_GPIO_BURNWIRE_ON, IOX_MODE_OUTPUT));
    WT_TRY(__burnwire_off());
    return WT_OK;
}

//-----------------------------------------------------------------------------
// Burnwire interface
//-----------------------------------------------------------------------------
int init_burnwire() {
    WTResult hal_result = __burnwire_init();
    if (hal_result != WT_OK) {
        char err_str[512];
        CETI_ERR("Failed to initialize the burnwire : %s", wt_strerror_r(hal_result, err_str, sizeof(err_str)));
        return -1;
    }

    CETI_LOG("Successfully initialized the burnwire");
    return 0;
}

int burnwireOn(void) {
    s_burnwire_led_state = 0;
    LEDCtrl_set_state(LED_STATE_BURN);
    WTResult hal_result = __burnwire_on();
    if (hal_result < 0) {
        char err_str[512];
        CETI_ERR("Failed to turn on the burnwire: %s", wt_strerror_r(hal_result, err_str, sizeof(err_str)));
        return (-1);
    }
    return 0;
}

int burnwireOff(void) {
    LEDCtrl_set_state(LED_STATE_FPGA);
    WTResult hal_result = __burnwire_off();
    if (hal_result < 0) {
        char err_str[512];
        CETI_ERR("Failed to turn off the burnwire: %s", wt_strerror_r(hal_result, err_str, sizeof(err_str)));
        return (-1);
    }
    return 0;
}
