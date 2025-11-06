#!/bin/sh
WESTON_PROCESS=`pgrep /usr/bin/weston`
if [ "$WESTON_PROCESS" == "" ]; then
    /usr/bin/systemctl restart weston.service
fi

