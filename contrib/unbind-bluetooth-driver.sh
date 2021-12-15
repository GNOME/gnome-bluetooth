#!/bin/sh

ADAPTER_TO_DISABLE=${1:-hci0}
SYSFS_PATH=/sys/class/bluetooth/$ADAPTER_TO_DISABLE

if [ ! -h $SYSFS_PATH ] ; then
	echo "Could not find adapter $ADAPTER_TO_DISABLE"
	echo "Usage: $0 [hciX]"
	exit 1
fi

USB_DEVICE_PATH=`realpath $SYSFS_PATH/device`
USB_DEVICE=`basename $USB_DEVICE_PATH`
echo $USB_DEVICE > $SYSFS_PATH/device/driver/unbind
