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
#include <stdlib.h> // for system
#include <sys/ioctl.h>

#include "logging.h"

void wifi_disable(void) {
  int sock = -1;
  struct ifreq wrq = {.ifr_name = WIFI_IFNAME};

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    CETI_ERR("Unable to open WiFi socket.");
    return;
  }

  if (ioctl(sock, SIOCGIFFLAGS, &wrq) < 0) {
    CETI_ERR("Unable to read wlan0 interface flags.");
    return;
  }

  // disable wifi
  wrq.ifr_flags &= ~IFF_UP;

  if (ioctl(sock, SIOCSIFFLAGS, &wrq) < 0) {
    CETI_ERR("Unable to write wlan0 interface flags.");
    return;
  }
}

void wifi_kill(void) {
  if (system("rfkill block wifi") != 0){
    CETI_ERR("Failed to kill wifi");
  }
}

void eth0_disable(void) {
  int sock = -1;
  struct ifreq wrq = {.ifr_name = ETHERNET_IFNAME};

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    CETI_ERR("Unable to open ethernet socket.");
    return;
  }

  if (ioctl(sock, SIOCGIFFLAGS, &wrq) < 0) {
    CETI_ERR("Unable to read eth0 interface flags.");
    return;
  }

  // disable eth0
  wrq.ifr_flags &= ~IFF_UP;

  if (ioctl(sock, SIOCSIFFLAGS, &wrq) < 0) {
    CETI_ERR("Unable to write eth0 interface flags.");
    return;
  }
}

void usb_kill(void) {
  // disable activity LED trigger
  int led_trigger_file = open(USB_SYSFS_PATH, O_WRONLY);
  if (led_trigger_file < 0) {
    CETI_ERR("Could not open usb power bus file: " USB_SYSFS_PATH);
  } else {
    if (write(led_trigger_file, "0", 1) != 1) {
      CETI_WARN("Write to %s failed", USB_SYSFS_PATH);
    }
    close(led_trigger_file);
  }
}

void bluetooth_kill(void) {
  if (system("rfkill block bluetooth") != 0) {
    CETI_WARN("Failed to kill bluetooth");
  }
}

void activity_led_disable(void) {
  // disable activity LED trigger
  int led_trigger_file = open(ACT_LED_TRIGGER_SYSFS_PATH, O_WRONLY);
  if (led_trigger_file < 0) {
    CETI_WARN("Could not open activity led trigger "
             "file: " ACT_LED_TRIGGER_SYSFS_PATH);
  } else {
    if (write(led_trigger_file, "none", 4) != 4){
      CETI_WARN("Write to %s failed", ACT_LED_TRIGGER_SYSFS_PATH);
    }

    close(led_trigger_file);
  }

  // turn off activity LED
  int led_brightness_file = open(ACT_LED_BRIGHTNESS_SYSFS_PATH, O_WRONLY);
  if (led_brightness_file < 0) {
    CETI_ERR("Could not open activity led brightness "
             "file: " ACT_LED_BRIGHTNESS_SYSFS_PATH);
  } else {
    if (write(led_brightness_file, "0", 1) != 1) {
      CETI_WARN("Write to %s failed", ACT_LED_BRIGHTNESS_SYSFS_PATH);
    }
    close(led_brightness_file);
  }
}

void activity_led_enable(void) {
  // disable activity LED trigger
  int led_trigger_file = open(ACT_LED_TRIGGER_SYSFS_PATH, O_WRONLY);
  if (led_trigger_file < 0) {
    CETI_WARN("Could not open activity led trigger "
             "file: " ACT_LED_TRIGGER_SYSFS_PATH);
  } else {
    if (write(led_trigger_file, "heartbeat", strlen("heartbeat")) != strlen("heartbeat")){
      CETI_WARN("Write to %s failed", ACT_LED_TRIGGER_SYSFS_PATH);
    }
    close(led_trigger_file);
  }

  // turn on activity LED
  int led_brightness_file = open(ACT_LED_BRIGHTNESS_SYSFS_PATH, O_WRONLY);
  if (led_brightness_file < 0) {
    CETI_WARN("Could not open activity led brightness "
             "file: " ACT_LED_BRIGHTNESS_SYSFS_PATH);
  } else {
    if (write(led_brightness_file, "1", 1) != 1) {
      CETI_WARN("Write to %s failed", ACT_LED_BRIGHTNESS_SYSFS_PATH);
    }
    close(led_brightness_file);
  }
}
