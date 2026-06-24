# Development

Developer notes for building and working on the plugin. Regular users do not need any of
this; see [README.md](README.md) to install and use the released plugin.

## Supported hardware

USB Vendor ID `0x1B1C` (Corsair). All variants speak the same USB HID command set; only
the buffer size differs.

| Controller | USB PID | HID buffer | Common products |
|---|---|---|---|
| Commander Core | `0x0C1C` | 96 bytes | Capellix AIOs (early revisions) |
| Commander Core 2 | `0x0C32` | 64 bytes | Capellix XT AIOs (2022+) |
| Commander Core 3 | `0x0C1D` | 96 bytes | |
| Commander Core 4 | `0x0C3C` | 96 bytes | |
| Commander Core 5 | `0x0C3D` | 96 bytes | |
| Commander Core 6 | `0x0C3E` | 96 bytes | |
| Commander Core XT | `0x0C2A` | 384 bytes | Standalone RGB controller |

Speed control (pump and fans) uses the firmware 2.x software-speed path on the
`0x0C32` controller, which is where liquidctl currently fails to write.

## Building from source

### 1. Clone

```bash
git clone https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core.git
cd openrgb-plugin-corsair-commander-core
```

### 2. Point at an OpenRGB source tree

The plugin builds against the OpenRGB headers. It must match the OpenRGB version you run.

```bash
export OPENRGB_DIR=/path/to/OpenRGB
```

### 3. Install build dependencies

```bash
# Arch Linux
sudo pacman -S qt5-base qt5-svg hidapi libusb mbedtls

# Debian/Ubuntu
sudo apt install build-essential qtbase5-dev qtbase5-dev-tools qt5-qmake \
    libqt5svg5-dev libusb-1.0-0-dev libhidapi-dev libmbedtls-dev pkgconf
```

### 4. Build

**Use Qt5 (`qmake`), not `qmake6`.** OpenRGB (1.0rc2) is a Qt5 application, and it
silently rejects a Qt6-built plugin with "Failed to extract plugin meta data". Confirm
`qmake --version` reports Qt 5.x.

```bash
qmake CorsairCommanderCore.pro     # Qt5 qmake, NOT qmake6
make -j$(nproc)
```

This produces `libOpenRGBCorsairCommanderCorePlugin.so`.

### 5. Install and reload

```bash
mkdir -p ~/.config/OpenRGB/plugins
cp libOpenRGBCorsairCommanderCorePlugin.so ~/.config/OpenRGB/plugins/
# restart OpenRGB (or your service that runs it)
```

## Known gotcha: device re-enumeration

Switching this controller to **hardware mode** can make it re-enumerate on the USB bus,
and the re-enumeration sometimes drops the `/dev/hidraw` node for interface 0. OpenRGB
uses the hidraw backend, so when that node is missing it can no longer open or detect the
device, even though the device is still present on USB (a libusb based tool like
liquidctl can still see it).

To avoid this the plugin no longer forces hardware mode when it unloads. If the device
still ends up missing from OpenRGB, recover it without a physical replug by bouncing the
USB device:

```bash
# find the USB path (for example 3-7)
for d in /sys/bus/usb/devices/*; do \
  [ "$(cat $d/idProduct 2>/dev/null)" = "0c32" ] && echo "$(basename $d)"; done
# unbind then bind it
echo 3-7 | sudo tee /sys/bus/usb/drivers/usb/unbind
echo 3-7 | sudo tee /sys/bus/usb/drivers/usb/bind
```

Both interfaces then get hidraw nodes again and OpenRGB can detect the device.

## CI / automated builds

Every push to `main` triggers a [GitHub Actions workflow](.github/workflows/build.yml)
that builds for Linux (x86_64, arm64, armhf), Windows (x86_64), and macOS (arm64, x86_64).
Artifacts are on the
[Actions tab](https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core/actions).
Pushing a version tag (`git tag v0.1.0 && git push --tags`) also creates a draft Release
with all binaries attached.

## Testing device communication without OpenRGB

A standalone Python script in `test/` talks to the device directly over `/dev/hidraw`
with no pip packages required.

```bash
cd test
sudo python3 test_colors.py   # sudo unless the udev rule is installed and the device replugged
```

Interactive commands: `solid <color>`, `pump <color>`, `fans <color>`,
`split <pump> <fans>`, `rainbow [seconds]`, `off`, `info`, `quit`. Colors are `#RRGGBB`
or `R,G,B`.

## Protocol reference

The Commander Core protocol here is a clean-room reimplementation based on protocol
documentation from:

- [OpenLinkHub](https://github.com/jurkovic-nikola/OpenLinkHub) (GPL-3.0), the Go
  implementation in `src/devices/cc/cc.go`.
- [liquidctl](https://github.com/liquidctl/liquidctl) (GPL-3.0), the Python implementation
  and its protocol docs.

This project is an independent MIT-licensed implementation. No code was copied from the
GPL-licensed reference projects; only the USB HID protocol (command bytes, packet layout,
endpoint sequences) was studied and reimplemented from scratch.
