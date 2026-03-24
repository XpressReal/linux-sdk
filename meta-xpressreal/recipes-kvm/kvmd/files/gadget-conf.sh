#!/bin/sh

# Get the current hostname
BOARD=$(hostname)

#echo "Current hostname is: $HOSTNAME"

# Use a case statement to handle different hostnames
case "$BOARD" in
    backinblack-rtd1619b)
        echo "Enable USB Port0 Device Mode for backinblack"
	echo device > /sys/class/usb_role/98013200.usb-port0-role-switch/role
        ;;
    smallville-rtd1625)
        echo "Enable USB Port2 Device Mode for phantom"
	echo device > /sys/class/usb_role/98013e00.usb-port2-role-switch/role
        ;;
    badassium-rtd1315c)
        echo "Handling USB Port for 1315C"
        # Put commands specific to boardC here
        ;;
    *)
        echo "Unknown board or hostname: $HOSTNAME"
        # Default or error handling
        ;;
esac
