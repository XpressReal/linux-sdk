#!/usr/bin/env python3
import subprocess
import os
import time
import struct
import threading
import sys

GPIO_CHIP    = "gpiochip0"
PIN_VOL_UP   = 19
PIN_VOL_DOWN = 20
PIN_POWER    = 21

# 2. USB JOYSTICK SETTINGS (Mute)
USB_EVENT_PATH = "/dev/input/event2"
CODE_MUTE      = 113

# 3. COMMANDS
CMD_VOL_UP   = "amixer sset Speaker 5%+"
CMD_VOL_DOWN = "amixer sset Speaker 5%-"
CMD_MUTE     = "amixer sset 'Speaker Channel' toggle"
CMD_SHUTDOWN = "shutdown -h now"


def run_cmd(name, cmd):
    print(f"[{name}] Triggered -> {cmd}")
    os.system(cmd)

# --- WORKER 1: VOLUME BUTTONS (Instant Trigger) ---
def monitor_simple_gpio(pin, name, command):
    print(f"[GPIO] Monitoring Pin {pin} for {name}...")
    while True:
        try:
            # Wait for Falling Edge (Press)
            subprocess.run(
                ["gpiomon", "--num-events=1", "--falling-edge", GPIO_CHIP, str(pin)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            # Button pressed
            run_cmd(name, command)
            time.sleep(0.25) # Debounce
        except Exception as e:
            print(f"Error on GPIO {pin}: {e}")
            time.sleep(1)

# --- WORKER 2: POWER BUTTON (Long Press Logic) ---
def monitor_power_button(pin):
    print(f"[Power] Monitoring Pin {pin} for 5-second hold...")
    while True:
        try:
            # 1. Sleep until button is pressed
            subprocess.run(
                ["gpiomon", "--num-events=1", "--falling-edge", GPIO_CHIP, str(pin)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )

            # 2. Button is down! Start counting duration.
            print("[Power] Button pressed... counting...")
            start_time = time.time()
            triggered = False

            while True:
                # Check current state using gpioget
                # We capture stdout directly to avoid 'print' spam
                state = subprocess.check_output(
                    ["gpioget", GPIO_CHIP, str(pin)]
                ).decode().strip()

                # If state is '1', button was released
                if state == '1':
                    duration = time.time() - start_time
                    print(f"[Power] Released after {duration:.2f}s (Too short)")
                    # OPTIONAL: Add 'if duration < 1: run_short_press_cmd()' here
                    break

                # Check if 3 seconds have passed
                elapsed = time.time() - start_time
                if elapsed >= 3:
                    print("[Power] 3 SECONDS REACHED! SHUTTING DOWN...")
                    run_cmd("Power", CMD_SHUTDOWN)
                    triggered = True
                    break

                # Wait a bit before checking again
                time.sleep(0.2)

            # If shutdown triggered, sleep loop to prevent repeat triggers
            if triggered:
                time.sleep(60)

        except Exception as e:
            print(f"Error on Power Pin: {e}")
            time.sleep(1)

# --- WORKER 3: USB MUTE (Event based) ---
def monitor_usb_mute():
    print(f"[USB] Monitoring {USB_EVENT_PATH} for Mute...")
    fmt = 'llHHi'
    size = struct.calcsize(fmt)
    try:
        f = open(USB_EVENT_PATH, "rb")
        while True:
            data = f.read(size)
            if data:
                (sec, usec, etype, ecode, evalue) = struct.unpack(fmt, data)
                if etype == 1 and evalue == 1 and ecode == CODE_MUTE:
                    run_cmd("MUTE", CMD_MUTE)
    except Exception as e:
        print(f"Error reading USB: {e}")

# --- MAIN ---
if __name__ == "__main__":
    # 1. Volume Up Thread
    t1 = threading.Thread(target=monitor_simple_gpio, args=(PIN_VOL_UP, "Vol UP", CMD_VOL_UP))

    # 2. Volume Down Thread
    t2 = threading.Thread(target=monitor_simple_gpio, args=(PIN_VOL_DOWN, "Vol DOWN", CMD_VOL_DOWN))

    # 3. USB Mute Thread
    t3 = threading.Thread(target=monitor_usb_mute)

    # 4. Power Button Thread
    t4 = threading.Thread(target=monitor_power_button, args=(PIN_POWER,))

    # Start all
    t1.start()
    t2.start()
    t3.start()
    t4.start()

    try:
        t1.join()
        t2.join()
        t3.join()
        t4.join()
    except KeyboardInterrupt:
        print("\nExiting.")
