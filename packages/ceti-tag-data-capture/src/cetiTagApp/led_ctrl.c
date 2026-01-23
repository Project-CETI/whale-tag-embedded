//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------
#include "led_ctrl.h"

#include "device/fpga.h"
#include "launcher.h"
#include "utils/timing.h"

#define LED_CTRL_UPDATE_INTERVAL_US (250000)
#define LED_CTRL_ERROR_RESULT_DISPLAY_INTERVAL_S (20)

static LEDState s_state = LED_STATE_FPGA;
static struct {
    LEDState return_state;
    size_t current_bit;
    size_t bit_len;
    uint32_t warn_flags;
    uint32_t err_flags;
} s_error = {0};

static uint8_t s_burnwire_led_state = 0;
static uint32_t s_dive_count = 0;

void LEDCtrl_set_state(LEDState state) {
    if (state == LED_STATE_EXIT_REPORT_ERROR) {
        s_state = LED_STATE_EXIT_REPORT_ERROR;
        LEDCtrl_set_state(s_error.return_state);
        return;
    }

    if (s_state == LED_STATE_REPORT_ERROR) {
        s_error.return_state = state;
        return;
    }

    switch (state) {
        case LED_STATE_FPGA:
            wt_fpga_led_release_all();
            break;

        case LED_STATE_BURN:
            wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
            wt_fpga_led_set(FPGA_LED_YELLOW, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
            wt_fpga_led_set(FPGA_LED_RED, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
            s_burnwire_led_state = 0;
            break;

        case LED_STATE_SHUTDOWN:
            wt_fpga_led_capture_all(FPGA_LED_STATE_OFF);
            break;

        case LED_STATE_REPORT_ERROR:
            wt_fpga_led_capture_all(FPGA_LED_STATE_OFF);
            break;

        case LED_STATE_DIVE:
            wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
            wt_fpga_led_set(FPGA_LED_YELLOW, FPGA_LED_MODE_FPGA_ONLY, FPGA_LED_STATE_OFF);
            wt_fpga_led_set(FPGA_LED_RED, FPGA_LED_MODE_FPGA_ONLY, FPGA_LED_STATE_OFF);
            s_dive_count = 0;
            break;

        case LED_STATE_EXIT_REPORT_ERROR:
            LEDCtrl_set_state(s_error.return_state);
            return;
    }
    s_state = state;
}

void LEDCtrl_flash_err(size_t bit_len, uint32_t warn, uint32_t err) {
    s_error.bit_len = bit_len;
    s_error.warn_flags = warn;
    s_error.err_flags = err;
    s_error.current_bit = 0;
    s_error.return_state = s_state;
    LEDCtrl_set_state(LED_STATE_REPORT_ERROR);
}

static void __LEDCtrl_task(void) {
    switch (s_state) {
        case LED_STATE_FPGA:
            break;

        case LED_STATE_SHUTDOWN:
            break;

        case LED_STATE_BURN:
            switch (s_burnwire_led_state) {
                case 0:
                    wt_fpga_led_set(FPGA_LED_YELLOW, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
                    wt_fpga_led_set(FPGA_LED_RED, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                    s_burnwire_led_state = 1;
                    break;

                case 1:
                    wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
                    wt_fpga_led_set(FPGA_LED_YELLOW, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                    s_burnwire_led_state = 2;
                    break;

                case 2:
                default:
                    wt_fpga_led_set(FPGA_LED_RED, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
                    wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                    s_burnwire_led_state = 0;
                    break;
            }
            break;

        case LED_STATE_REPORT_ERROR: {
            static int error_hold_count = 0;
            if ((s_error.current_bit >> 1) < s_error.bit_len) {
                /* BLINK ERROR CODE */
                if ((s_error.current_bit & 1)) {
                    /* BLINK ON */
                    // Clock with Yellow
                    wt_fpga_led_set(FPGA_LED_YELLOW, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
                    // red for critical
                    if (s_error.err_flags & (1 << (s_error.current_bit >> 1))) {
                        wt_fpga_led_set(FPGA_LED_RED, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
                    }
                    // green non-critical
                    if (s_error.warn_flags & (1 << (s_error.current_bit >> 1))) {
                        wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
                    }
                } else {
                    /* BLINK OFF */
                    wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                    wt_fpga_led_set(FPGA_LED_YELLOW, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                    wt_fpga_led_set(FPGA_LED_RED, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                }
                s_error.current_bit++;
            } else if ((s_error.current_bit >> 1) == s_error.bit_len) {
                /* HOLD ERROR LEVEL */
                if ((s_error.current_bit & 1)) {
                    wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                    wt_fpga_led_set(FPGA_LED_YELLOW, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                    wt_fpga_led_set(FPGA_LED_RED, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
                    s_error.current_bit++;
                } else {
                    if (0 != s_error.err_flags) {
                        wt_fpga_led_set(FPGA_LED_RED, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
                    }
                    wt_fpga_led_set(FPGA_LED_YELLOW, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
                    s_error.current_bit = (s_error.bit_len + 1) << 1;
                    error_hold_count = 0;
                }
            } else if ((s_error.current_bit >> 1) > s_error.bit_len) {
                /* TRANSITION TO NEXT STATE*/
                error_hold_count++;
                if (error_hold_count > (4 * LED_CTRL_ERROR_RESULT_DISPLAY_INTERVAL_S)) {
                    LEDCtrl_set_state(LED_STATE_EXIT_REPORT_ERROR);
                }
            }
            break;
        }

        case LED_STATE_DIVE: {
            /* Blink once every 10 seconds */
            if (s_dive_count == 0) {
                wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_ON);
            } else {
                wt_fpga_led_set(FPGA_LED_GREEN, FPGA_LED_MODE_PI_ONLY, FPGA_LED_STATE_OFF);
            }
            s_dive_count = (s_dive_count + 1) % (4 * 10);
            break;
        }

        case LED_STATE_EXIT_REPORT_ERROR: {
            LEDCtrl_set_state(LED_STATE_FPGA);
            break;
        }
    }
}

void *LEDCtrl_thread(void *paramPtr) {
    while (!g_exit) {
        int64_t task_start_us = get_monotonic_time_us();

        __LEDCtrl_task();

        int64_t elapsed_time_us = get_monotonic_time_us() - task_start_us;
        int64_t sleep_duration_us = LED_CTRL_UPDATE_INTERVAL_US - elapsed_time_us;
        if (0 < sleep_duration_us) {
            usleep(sleep_duration_us);
        }
    }
    return 0;
}