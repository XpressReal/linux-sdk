#!/usr/bin/env python3
import os, sys

LUN_FILE = "/sys/kernel/config/usb_gadget/kvmd/functions/mass_storage.usb0/lun.0/file"
UDC = "/sys/kernel/config/usb_gadget/kvmd/UDC"

def main():
    if len(sys.argv) != 2:
        print("Usage: kvmd-set-lun-file <backing_file>")
        sys.exit(1)
    backing = sys.argv[1]

    # disable gadget
    #with open(UDC, "w") as f:
    #    f.write("")

    # clear old backing
    with open(LUN_FILE, "w") as f:
        f.write("")

    # set new file
    with open(LUN_FILE, "w") as f:
        f.write(backing)

    # enable gadget
    udc_list = os.listdir("/sys/class/udc")
    if not udc_list:
        print("No UDC available", file=sys.stderr)
        sys.exit(1)
    udc_name = udc_list[0]
    #with open(UDC, "w") as f:
    #    f.write(udc_name)

    print("OK: LUN now uses", backing)

if __name__ == "__main__":
    main()

