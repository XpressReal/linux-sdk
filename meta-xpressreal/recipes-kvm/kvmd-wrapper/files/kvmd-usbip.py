#!/usr/bin/env python3

import subprocess
import sys
import re
import time

from pathlib import Path

def get_remote_busids(host):
    """Return list of exported busids from remote usbip host"""
    result = subprocess.run(
        ["/sbin/usbip", "list", "-r", host],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"[ERROR] Cannot connect to {host}")
        print(result.stderr.strip())
        sys.exit(1)

    busids = []
    for line in result.stdout.splitlines():
        if "error:" in line.lower() or "warning:" in line.lower():
            continue
        m = re.match(r"^\s*([\d\-.]+):", line)
        if m:
            busids.append(m.group(1))
    return busids

def get_attached_ports():
    """Return list of currently attached port numbers (e.g. ['08', '09'])"""
    result = subprocess.run(["/sbin/usbip", "port"], capture_output=True, text=True)
    ports = []
    for line in result.stdout.splitlines():
        m = re.match(r"^Port\s+(\d{2}):", line)
        if m:
            ports.append(m.group(1))
    return ports

def wait_for_sda_fixed(timeout=30):
    """Wait until /dev/sda exists, has at least one partition and is readable"""
    print("[WAIT] Waiting for /dev/sda to become ready...", end="", flush=True)
    start = time.time()

    while time.time() - start < timeout:
        if not Path("/dev/sda").exists():
            time.sleep(0.5)
            print(".", end="", flush=True)
            continue

        # Wait for at least one partition (sda1..sda15)
        if any(Path(f"/dev/sda{i}").exists() for i in range(1, 16)):
            try:
                with open("/dev/sda", "rb") as f:
                    f.read(1)                     # real read test
                print("\n[OK] /dev/sda is ready and usable!")
                return True
            except OSError:
                pass

        time.sleep(0.6)
        print(".", end="", flush=True)

    print("\n[ERROR] Timeout - /dev/sda did not become ready")
    return False

def do_attach(host):
    busids = get_remote_busids(host)
    if not busids:
        print("[ERROR] No exported USB devices on remote host")
        sys.exit(1)
    busid = busids[0]
    print(f"[INFO] Attaching {host} → busid {busid}")
    subprocess.run(["/sbin/usbip", "attach", "-r", host, "-b", busid])

    wait_for_sda_fixed(timeout=30)

def do_detach(port=None):
    if port:
        print(f"[INFO] Detaching port {port}")
        subprocess.run(["/sbin/usbip", "detach", "-p", port])
        return

    ports = get_attached_ports()
    if not ports:
        print("[INFO] No usbip ports currently in use")
        return

    print(f"[INFO] Found {len(ports)} attached port(s): {', '.join(ports)}")
    for p in ports:
        print(f"[INFO] Detaching port {p}")
        subprocess.run(["/sbin/usbip", "detach", "-p", p])

# ====================== Main ======================
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:")
        print("  usbip_tool.py attach <IP>        # attach first available device")
        print("  usbip_tool.py detach [PORT]      # detach all or specific port")
        sys.exit(1)

    command = sys.argv[1].lower()

    if command in ("attach", "a"):
        if len(sys.argv) != 3:
            print("attach needs remote IP")
            sys.exit(1)
        do_attach(sys.argv[2])

    elif command in ("detach", "d"):
        if len(sys.argv) == 2:
            do_detach()                 # auto detach all
        elif len(sys.argv) == 3:
            do_detach(sys.argv[2])      # detach specific port
        else:
            print("detach usage: detach [PORT]")
            sys.exit(1)

    else:
        print(f"Unknown command: {command}")
        sys.exit(1)
