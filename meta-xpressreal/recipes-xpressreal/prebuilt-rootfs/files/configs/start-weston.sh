#!/bin/sh

NAME="${DEVPATH##*/}"
NAME="${NAME#card*-}"

STATUS="`/usr/bin/cat /sys$DEVPATH/$NAME-HDMI-A-1/status`"

WESTON_PROCESS=`/usr/bin/pgrep weston`
if [ "$WESTON_PROCESS" != "" ] && [ "$STATUS" != "disconnected" ]; then
        /usr/bin/systemctl restart weston.service
fi
