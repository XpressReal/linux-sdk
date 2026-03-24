#!/bin/sh

# Interfaces to check, in order of priority
INTERFACES="eth0 wlan0"

# Find the first interface with an IP address
for IFACE in $INTERFACES; do
    IP_ADDR=$(/usr/bin/ip -4 addr show "$IFACE" 2>/dev/null | /usr/bin/awk '/inet / {print $2}' | cut -d/ -f1 | head -n1)
    if [ -n "$IP_ADDR" ]; then
        echo "Found IP on $IFACE: $IP_ADDR"
        break
    fi
done

# If no IP address found
if [ -z "$IP_ADDR" ]; then
    echo "No IP address found on interfaces: $INTERFACES"
    IP_ADDR="192.168.8.138"
    #exit 1
fi

# Generate QR code
WIFI="WIFI:T:WPA;S:GL_AXT1800-4fc-5G;P:A12345678b;;"
OUTPUT1="/tmp/wifi_qr.png"
qrencode -o "$OUTPUT1" -s 10 -m 3 "$WIFI"

URL="http://admin:admin@${IP_ADDR}"
OUTPUT2="/tmp/ip_qr.png"
qrencode -o "$OUTPUT2" -s 10 -m 3 "$URL"

BGFILE="/etc/kvmd/background.png"
WESTON_TMPFILE="/tmp/weston_bg.png"
WESTON_BGFILE="/etc/xdg/weston/logo_banner.jpg"

/usr/bin/composite -geometry +267+232 $OUTPUT1 $BGFILE $WESTON_TMPFILE
/usr/bin/composite -geometry +1177+353 $OUTPUT2 $WESTON_TMPFILE $WESTON_BGFILE

#echo "QR code generated at: $OUTPUT"

