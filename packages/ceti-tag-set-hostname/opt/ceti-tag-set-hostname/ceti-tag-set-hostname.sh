#!/bin/sh
# shellcheck disable=SC2010

# Try to see if there's a wireless network adapter in the system
# If there is, use its MAC as a unique identifier
NETWORK_ADAPTER_NAME="$(ls /sys/class/net/ | grep wl | head -n1 | awk '{print $1;}')"

# If wireless adapters are not present, 
# try to use ethernet adapter's MAC
if [ -z "$NETWORK_ADAPTER_NAME" ]; then
	NETWORK_ADAPTER_NAME="$(ls /sys/class/net/ | grep ^e | head -n1 | awk '{print $1;}')"
fi


# Now assuming we do have an Ethernet or Wireless adapter,
# try to get its MAC address
MAC_ADDRESS="/sys/class/net/$NETWORK_ADAPTER_NAME/address"
if [ -f "$MAC_ADDRESS" ]; then
	DEVICE_ID="$(< "$MAC_ADDRESS" sed s/':'//g)"
fi

# If we still were unable to get MAC address of any adapter,
# try to use Serial number from /proc/cpuinfo.
# Note that there are no x86-64 systems that have it,
# but raspberry pi does.
if [ -z "$DEVICE_ID" ]; then
	DEVICE_ID="$(< /proc/cpuinfo grep Serial | sed s/'.*:\s0*'/''/)"
fi

# If we still have no UID, just give up
if [ -z "$DEVICE_ID" ]; then
    echo "Unable to find an identifier for the hw, exiting"
    exit 1
fi

# Set device is as the hostname
DEVICE_ID="wt-$DEVICE_ID"
HOSTNAME="$(hostname)"

if [ "$HOSTNAME" != "$DEVICE_ID" ]; then
    echo "Changing hostname to $DEVICE_ID"
    hostname "$DEVICE_ID"
    echo "127.0.0.1 $DEVICE_ID" >> /etc/hosts
    sysctl kernel.hostname="$DEVICE_ID"
fi
