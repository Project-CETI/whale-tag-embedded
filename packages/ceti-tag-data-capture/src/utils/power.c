//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg,
//-----------------------------------------------------------------------------

#include "power.h"

#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "logging.h"

void wifi_disable(void) {
  int sock = -1;
  struct ifreq wrq = {.ifr_name = WIFI_IFNAME};

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    CETI_LOG("Unable to open WiFi socket.");
    return;
  }

  if (ioctl(sock, SIOCGIFFLAGS, &wrq) < 0) {
    CETI_LOG("Unable to read wlan0 interface flags.");
    return;
  }

  // disable wifi
  wrq.ifr_flags &= ~IFF_UP;

  if (ioctl(sock, SIOCSIFFLAGS, &wrq) < 0) {
    CETI_LOG("Unable to write wlan0 interface flags.");
    return;
  }
}

void usb_disable(void) {
  // disable activity LED trigger
  int led_trigger_file = open(USB_SYSFS_PATH, O_WRONLY);
  if (led_trigger_file < 0) {
    CETI_LOG("Could not open usb power bus file: " USB_SYSFS_PATH);
  } else {
    write(led_trigger_file, "0", 1);
    close(led_trigger_file);
  }
}

void activity_led_disable(void) {
  // disable activity LED trigger
  int led_trigger_file = open(ACT_LED_TRIGGER_SYSFS_PATH, O_WRONLY);
  if (led_trigger_file < 0) {
    CETI_LOG("Could not open activity led trigger "
             "file: " ACT_LED_TRIGGER_SYSFS_PATH);
  } else {
    write(led_trigger_file, "none", 4);
    close(led_trigger_file);
  }

  // turn off activity LED
  int led_brightness_file = open(ACT_LED_BRIGHTNESS_SYSFS_PATH, O_WRONLY);
  if (led_brightness_file < 0) {
    CETI_LOG("Could not open activity led brightness "
             "file: " ACT_LED_BRIGHTNESS_SYSFS_PATH);
  } else {
    write(led_brightness_file, "0", 1);
    close(led_brightness_file);
  }
}
