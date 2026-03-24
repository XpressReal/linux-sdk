#!/bin/sh

CONFIG_FILE="/etc/kvmd/override.d/gpio.yaml"
BOARD=$(hostname)

case "$BOARD" in
    backinblack-rtd1619b)
        NEW_PIN_USB_BREAKER=22
        NEW_PIN_CONST1=80
        NEW_PIN_LOCATOR=12
        ;;
    smallville-rtd1625)
        NEW_PIN_USB_BREAKER=19
        NEW_PIN_CONST1=136
        NEW_PIN_LOCATOR=12
        ;;
    badassium-rtd1315c)
        NEW_PIN_USB_BREAKER=42
        NEW_PIN_CONST1=84
        NEW_PIN_LOCATOR=19
        ;;
    *)
        echo "Unknown board $BOARD, using default pins"
        NEW_PIN_USB_BREAKER=22
        NEW_PIN_CONST1=80
        NEW_PIN_LOCATOR=12
        ;;
esac

cp "$CONFIG_FILE" "$CONFIG_FILE.bak"

sed -i -e '
  # For __v3_usb_breaker__ section
  /__v3_usb_breaker__:/,/pin:/ {
    /^ *# pin: null$/ {
      n
      s/^ *pin: .*/                pin: '"$NEW_PIN_USB_BREAKER"'/
    }
  }
  # For __v4_const1__ section
  /__v4_const1__:/,/pin:/ {
    /^ *# pin: null$/ {
      n
      s/^ *pin: .*/                pin: '"$NEW_PIN_CONST1"'/
    }
  }
  # For __v4_locator__ section
  /__v4_locator__:/,/pin:/ {
    /^ *# pin: null$/ {
      n
      s/^ *pin: .*/                pin: '"$NEW_PIN_LOCATOR"'/
    }
  }
' "$CONFIG_FILE"

#echo "Pin lines updated based on '# pin: null' marker for board $BOARD"
