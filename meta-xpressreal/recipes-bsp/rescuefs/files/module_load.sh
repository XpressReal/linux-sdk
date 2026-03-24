#!/bin/sh
# Load kernel modules early in boot

MODULES_FILE=/etc/modules

[ -r "$MODULES_FILE" ] || exit 0

while read mod args; do
    case "$mod" in
        ""|\#*) continue ;;  # skip empty lines and comments
    esac

    # Try to load module
    insmod /lib/modules/"$mod".ko $args 2>/dev/null || echo "Failed to load $mod"
done < "$MODULES_FILE"
