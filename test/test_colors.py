#!/usr/bin/env python3
"""
Interactive RGB test for Corsair iCUE H150i Elite CAPELLIX XT.

Communicates with the Commander Core / Commander ST controller via
/dev/hidraw using the corrected protocol (0x08 header byte at position 1).

Usage:
    sudo $(which python) test_colors.py

Requires no pip packages — uses direct /dev/hidraw I/O.
"""

import os
import sys
import time
import fcntl
import select
import struct
import colorsys

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

USBDEVFS_RESET = 0x5514

# Buffer sizes for Commander ST (PID 0x0C32) — 64-byte reports
BUF_SIZE = 64
WRITE_BUF_SIZE = 65         # +1 for HID report ID
HEADER_SIZE = 2              # report_id + 0x08
HEADER_WRITE_SIZE = 4        # header in writeColor buffer construction
LED_START_INDEX = 6          # LED data offset in read response
MAX_BUF_PER_REQUEST = 61    # max color payload chunk (64-byte buffer)

# Protocol commands (endpoint parameter to transfer())
CMD_OPEN_ENDPOINT       = bytes([0x0D, 0x01])
CMD_OPEN_COLOR_ENDPOINT = bytes([0x0D, 0x00])
CMD_CLOSE_ENDPOINT      = bytes([0x05, 0x01, 0x01])
CMD_GET_FIRMWARE        = bytes([0x02, 0x13])
CMD_SOFTWARE_MODE       = bytes([0x01, 0x03, 0x00, 0x02])
CMD_HARDWARE_MODE       = bytes([0x01, 0x03, 0x00, 0x01])
CMD_WRITE_COLOR         = bytes([0x06, 0x00])
CMD_WRITE_COLOR_NEXT    = bytes([0x07, 0x00])
CMD_READ                = bytes([0x08, 0x01])

# Data modes (buffer parameter to transfer())
MODE_GET_LEDS           = bytes([0x20])
MODE_SET_COLOR          = bytes([0x22])

# Data type prefix for color writes
DATA_TYPE_SET_COLOR     = bytes([0x12, 0x00])

# Known Corsair USB PIDs for Commander Core variants
KNOWN_PIDS = ["0C1C", "0C32"]


# ---------------------------------------------------------------------------
# Device discovery
# ---------------------------------------------------------------------------

def find_hidraw_iface0():
    """Find the /dev/hidraw path for interface 0 of the Commander Core."""
    for name in sorted(os.listdir("/sys/class/hidraw/")):
        uevent_path = f"/sys/class/hidraw/{name}/device/uevent"
        if not os.path.exists(uevent_path):
            continue
        with open(uevent_path) as f:
            content = f.read().upper()
        if "1B1C" not in content:
            continue
        if not any(pid in content for pid in KNOWN_PIDS):
            continue

        device_link = os.path.realpath(f"/sys/class/hidraw/{name}/device")
        for parent in [device_link, os.path.dirname(device_link)]:
            p = os.path.join(parent, "bInterfaceNumber")
            if os.path.exists(p):
                with open(p) as f:
                    iface = f.read().strip()
                if iface == "00":
                    return f"/dev/{name}"
    return None


def find_usb_device():
    """Find the /dev/bus/usb path for USB reset."""
    for name in sorted(os.listdir("/sys/class/hidraw/")):
        uevent_path = f"/sys/class/hidraw/{name}/device/uevent"
        if not os.path.exists(uevent_path):
            continue
        with open(uevent_path) as f:
            content = f.read().upper()
        if "1B1C" not in content:
            continue
        if not any(pid in content for pid in KNOWN_PIDS):
            continue

        device_link = os.path.realpath(f"/sys/class/hidraw/{name}/device")
        parts = device_link.split("/")
        for i in range(len(parts) - 1, 0, -1):
            usb_path = "/".join(parts[:i + 1])
            busnum_path = os.path.join(usb_path, "busnum")
            devnum_path = os.path.join(usb_path, "devnum")
            if os.path.exists(busnum_path) and os.path.exists(devnum_path):
                with open(busnum_path) as f:
                    busnum = int(f.read().strip())
                with open(devnum_path) as f:
                    devnum = int(f.read().strip())
                return f"/dev/bus/usb/{busnum:03d}/{devnum:03d}"
    return None


def usb_reset():
    """USB reset the Commander Core to clear any stuck state."""
    usb_dev = find_usb_device()
    if not usb_dev:
        return False
    try:
        fd = os.open(usb_dev, os.O_WRONLY)
        fcntl.ioctl(fd, USBDEVFS_RESET, 0)
        os.close(fd)
        time.sleep(3)
        return True
    except Exception:
        return False


# ---------------------------------------------------------------------------
# Commander Core driver
# ---------------------------------------------------------------------------

class CommanderCore:
    """
    Commander Core protocol driver using direct /dev/hidraw I/O.

    Write packet layout (65 bytes):
      [0] = 0x00  (HID report ID)
      [1] = 0x08  (fixed protocol header)
      [2..2+len(endpoint)] = command bytes
      [2+len(endpoint)..] = buffer/payload bytes
      ... zero-padded to 65 bytes
    """

    def __init__(self):
        self.fd = None
        self.hidraw_path = None
        self.channels = {}       # channel_index -> led_count
        self.total_leds = 0
        self.firmware = "unknown"

    # -- low-level I/O -----------------------------------------------------

    def transfer(self, endpoint: bytes, buf: bytes = b"") -> bytes | None:
        """Send a command and read the response. Matches OpenLinkHub transfer()."""
        pkt = bytearray(WRITE_BUF_SIZE)
        pkt[0] = 0x00  # HID report ID
        pkt[1] = 0x08  # Fixed protocol header
        ep_start = HEADER_SIZE
        pkt[ep_start:ep_start + len(endpoint)] = endpoint
        if buf:
            buf_start = ep_start + len(endpoint)
            pkt[buf_start:buf_start + len(buf)] = buf

        os.write(self.fd, bytes(pkt))
        time.sleep(0.005)

        ready, _, _ = select.select([self.fd], [], [], 2.0)
        if ready:
            return os.read(self.fd, 512)
        return None

    def read_endpoint(self, mode: bytes) -> bytes | None:
        """Read from a data endpoint: close-open-read-close."""
        self.transfer(CMD_CLOSE_ENDPOINT, mode)
        self.transfer(CMD_OPEN_ENDPOINT, mode)
        resp = self.transfer(CMD_READ, mode)
        self.transfer(CMD_CLOSE_ENDPOINT, mode)
        return resp

    # -- device lifecycle --------------------------------------------------

    def open(self):
        """Find and open the Commander Core hidraw device."""
        print("[*] Scanning for Commander Core ...")

        hidraw = find_hidraw_iface0()
        if not hidraw:
            print("[!] Commander Core not found.")
            print("    Attempting USB reset ...")
            if usb_reset():
                hidraw = find_hidraw_iface0()

        if not hidraw:
            print("[!] Device not found. Check lsusb | grep 1b1c")
            sys.exit(1)

        self.hidraw_path = hidraw
        self.fd = os.open(hidraw, os.O_RDWR)
        print(f"    Opened: {hidraw}")

    def close(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def initialize(self):
        """Full initialization sequence matching OpenLinkHub cc.go Init()."""

        # 1) Get firmware
        print("[*] Reading firmware ...")
        resp = self.transfer(CMD_GET_FIRMWARE)
        if resp and len(resp) >= 7:
            v1 = resp[3]
            v2 = resp[4]
            v3 = struct.unpack_from("<H", resp, 5)[0]
            self.firmware = f"v{v1}.{v2}.{v3}"
        print(f"    Firmware: {self.firmware}")

        # 2) Enter software mode
        print("[*] Entering software control mode ...")
        self.transfer(CMD_SOFTWARE_MODE)

        # 3) Init LED ports (0x14, port, 0x01 for ports 0-6)
        print("[*] Initializing LED ports ...")
        for i in range(7):
            self.transfer(bytes([0x14, i, 0x01]))
        time.sleep(0.5)

        # 4) Query LED configuration
        print("[*] Querying LED channels ...")
        resp = self.read_endpoint(MODE_GET_LEDS)
        self._parse_led_config(resp)

        # 5) Open color endpoint for writing
        print("[*] Opening color endpoint ...")
        self.transfer(CMD_CLOSE_ENDPOINT, MODE_SET_COLOR)
        self.transfer(CMD_OPEN_COLOR_ENDPOINT, MODE_SET_COLOR)

    def _parse_led_config(self, resp: bytes | None):
        """Parse LED channel data: 4 bytes per channel starting at byte 6."""
        self.channels = {}
        self.total_leds = 0

        if not resp or len(resp) < LED_START_INDEX + 4:
            print("    WARN: No LED data, using defaults (33+8+8+8)")
            self.channels = {0: 33, 1: 8, 2: 8, 3: 8}
            self.total_leds = 57
            return

        device_names = {
            4: "ML PRO RGB fan", 8: "AF/SP RGB Elite fan",
            10: "RGB LED Strip", 12: "HD RGB fan",
            16: "LL RGB fan", 21: "Pump head (21)",
            24: "Pump head (24)", 29: "Pump head (29)",
            33: "CAPELLIX pump head", 34: "QL RGB fan",
        }

        for ch in range(7):
            off = LED_START_INDEX + ch * 4
            if off + 3 >= len(resp):
                break
            status = resp[off]
            num_leds = struct.unpack_from("<H", resp, off + 2)[0]
            if status == 0x02 and num_leds > 0:
                self.channels[ch] = num_leds
                self.total_leds += num_leds
                name = device_names.get(num_leds, f"Unknown ({num_leds} LEDs)")
                print(f"    Channel {ch}: {num_leds:3d} LEDs  ({name})")

        if self.total_leds == 0:
            print("    WARN: No LEDs detected, using defaults")
            self.channels = {0: 33, 1: 8, 2: 8, 3: 8}
            self.total_leds = 57

        print(f"    Total: {self.total_leds} LEDs")

    # -- color output ------------------------------------------------------

    def send_colors(self, color_data: bytes):
        """
        Send RGB color data matching OpenLinkHub writeColor().

        color_data: flat [R,G,B,R,G,B,...] for all LEDs in channel order.
        """
        # Build write buffer: [size_le_u16, 0x00, 0x00, dataType, color_data]
        size = len(color_data) + 2
        write_buf = bytearray()
        write_buf += struct.pack("<H", size)
        write_buf += bytes([0x00, 0x00])
        write_buf += DATA_TYPE_SET_COLOR
        write_buf += color_data

        # Chunk into MAX_BUF_PER_REQUEST-sized pieces
        chunks = []
        offset = 0
        while offset < len(write_buf):
            chunk_size = min(MAX_BUF_PER_REQUEST, len(write_buf) - offset)
            chunks.append(bytes(write_buf[offset:offset + chunk_size]))
            offset += chunk_size

        # Send chunks
        for i, chunk in enumerate(chunks):
            if i == 0:
                self.transfer(CMD_WRITE_COLOR, chunk)
            else:
                self.transfer(CMD_WRITE_COLOR_NEXT, chunk)

    def set_solid_color(self, r: int, g: int, b: int):
        """Set every LED to the same color."""
        self.send_colors(bytes([r, g, b] * self.total_leds))

    def set_channel_colors(self, channel_colors: dict[int, tuple[int, int, int]]):
        """Set each channel to a different solid color."""
        buf = bytearray()
        for ch in sorted(self.channels.keys()):
            count = self.channels[ch]
            color = channel_colors.get(ch, (0, 0, 0))
            buf += bytes(list(color) * count)
        self.send_colors(bytes(buf))

    def rainbow_cycle(self, duration: float = 10.0, speed: float = 1.0):
        """Animate a rainbow sweep across all LEDs."""
        fps = 30
        frames = int(duration * fps)
        print(f"    Playing rainbow for {duration}s ... (Ctrl+C to stop)")
        try:
            for frame in range(frames):
                t = frame / fps * speed
                buf = bytearray()
                for i in range(self.total_leds):
                    hue = (t + i / self.total_leds) % 1.0
                    r, g, b = colorsys.hsv_to_rgb(hue, 1.0, 1.0)
                    buf += bytes([int(r * 255), int(g * 255), int(b * 255)])
                self.send_colors(bytes(buf))
                time.sleep(1.0 / fps)
        except KeyboardInterrupt:
            print("\n    Stopped.")

    def shutdown(self):
        """Return to hardware control mode."""
        print("[*] Returning to hardware control mode ...")
        self.transfer(CMD_HARDWARE_MODE)


# ---------------------------------------------------------------------------
# Interactive console
# ---------------------------------------------------------------------------

def parse_color(text: str) -> tuple[int, int, int] | None:
    """Parse color from #RRGGBB hex or R,G,B decimal."""
    text = text.strip()
    if text.startswith("#") and len(text) == 7:
        try:
            return (int(text[1:3], 16), int(text[3:5], 16), int(text[5:7], 16))
        except ValueError:
            return None
    parts = text.replace(" ", ",").split(",")
    if len(parts) == 3:
        try:
            return (int(parts[0]), int(parts[1]), int(parts[2]))
        except ValueError:
            return None
    return None


def interactive(cc: CommanderCore):
    print()
    print("=" * 60)
    print("  Corsair H150i Elite CAPELLIX XT - RGB Test Console")
    print("=" * 60)
    print()
    print("Commands:")
    print("  solid <color>          Set all LEDs (e.g. solid #FF0000)")
    print("  pump <color>           Set pump head only")
    print("  fans <color>           Set all fans only")
    print("  split <pump> <fans>    Pump and fans different colors")
    print("  rainbow [seconds]      Rainbow animation (default 10s)")
    print("  off                    Turn all LEDs off (black)")
    print("  info                   Show device info")
    print("  quit                   Exit (returns to hardware mode)")
    print()
    print("Colors: #RRGGBB or R,G,B  (e.g. #00FF00 or 0,255,0)")
    print()

    while True:
        try:
            line = input(">>> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if not line:
            continue

        parts = line.split(maxsplit=1)
        cmd = parts[0].lower()
        args = parts[1] if len(parts) > 1 else ""

        if cmd in ("quit", "exit", "q"):
            break

        elif cmd == "solid":
            color = parse_color(args)
            if color:
                print(f"    All LEDs -> #{color[0]:02X}{color[1]:02X}{color[2]:02X}")
                cc.set_solid_color(*color)
            else:
                print("    Usage: solid #FF0000")

        elif cmd == "pump":
            color = parse_color(args)
            if color:
                colors = {ch: (0, 0, 0) for ch in cc.channels}
                colors[0] = color
                print(f"    Pump head -> #{color[0]:02X}{color[1]:02X}{color[2]:02X}")
                cc.set_channel_colors(colors)
            else:
                print("    Usage: pump #00FF00")

        elif cmd == "fans":
            color = parse_color(args)
            if color:
                colors = {ch: color if ch != 0 else (0, 0, 0) for ch in cc.channels}
                print(f"    Fans -> #{color[0]:02X}{color[1]:02X}{color[2]:02X}")
                cc.set_channel_colors(colors)
            else:
                print("    Usage: fans #0000FF")

        elif cmd == "split":
            tokens = args.split()
            if len(tokens) >= 2:
                pc, fc = parse_color(tokens[0]), parse_color(tokens[1])
                if pc and fc:
                    colors = {ch: pc if ch == 0 else fc for ch in cc.channels}
                    print(f"    Pump -> #{pc[0]:02X}{pc[1]:02X}{pc[2]:02X}  "
                          f"Fans -> #{fc[0]:02X}{fc[1]:02X}{fc[2]:02X}")
                    cc.set_channel_colors(colors)
                else:
                    print("    Usage: split #FF0000 #0000FF")
            else:
                print("    Usage: split <pump_color> <fan_color>")

        elif cmd == "rainbow":
            dur = 10.0
            if args:
                try:
                    dur = float(args)
                except ValueError:
                    pass
            cc.rainbow_cycle(duration=dur)

        elif cmd == "off":
            print("    LEDs off (black)")
            cc.set_solid_color(0, 0, 0)

        elif cmd == "info":
            print(f"    Firmware: {cc.firmware}")
            print(f"    Device:   {cc.hidraw_path}")
            print(f"    LEDs:     {cc.total_leds}")
            for ch, count in sorted(cc.channels.items()):
                print(f"    Channel {ch}: {count} LEDs")

        else:
            print(f"    Unknown command: {cmd}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if os.getuid() != 0:
        print("[!] Must run as root. Try: sudo $(which python) test_colors.py")
        sys.exit(1)

    cc = CommanderCore()
    try:
        cc.open()
        cc.initialize()
        interactive(cc)
    except Exception as e:
        print(f"[!] Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        print()
        try:
            cc.shutdown()
        except Exception:
            pass
        cc.close()
        print("[*] Done.")


if __name__ == "__main__":
    main()
