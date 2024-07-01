//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------

#ifndef CETI_POWER_H
#define CETI_POWER_H

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------
#define WIFI_IFNAME "wlan0"
#define ETHERNET_IFNAME "eth0"
#define USB_SYSFS_PATH "/sys/devices/platform/soc/3f980000.usb/buspower"
#define ACT_LED_TRIGGER_SYSFS_PATH "/sys/class/leds/ACT/trigger"
#define ACT_LED_BRIGHTNESS_SYSFS_PATH "/sys/class/leds/ACT/brightness"

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
void wifi_disable(void);
void wifi_kill(void);
void eth0_disable(void);
void usb_kill(void);
void bluetooth_kill(void);
void activity_led_disable(void);
void activity_led_enable(void);

#endif