> **Warning**: This is an AI-generated project. The code was authored by an AI assistant
> (Claude) based on reverse-engineered protocol documentation from the
> [OpenLinkHub](https://github.com/jurkovic-nikola/OpenLinkHub) and
> [liquidctl](https://github.com/liquidctl/liquidctl) projects. It has not been
> extensively tested across all hardware revisions or firmware versions. Use at your own
> risk, and please verify behavior on your specific hardware before relying on it.

# OpenRGB Plugin: Corsair Commander Core

An [OpenRGB](https://openrgb.org/) plugin that replaces OpenRGB's implementation
of the Corsair Commander Core controllers by including support for the Capellix
and Capellix XT series AIO liquid CPU coolers.

## Quick Start

1. **Download** the plugin for your platform from the
   [Releases](https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core/releases)
   page or the [Actions](https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core/actions)
   tab (latest build):

   | Platform | File |
   |---|---|
   | Linux | `libOpenRGBCorsairCommanderCorePlugin.so` |
   | Windows | `OpenRGBCorsairCommanderCorePlugin.dll` |
   | macOS | `libOpenRGBCorsairCommanderCorePlugin.dylib` |

2. **Disable the built-in Commander Core detector** — OpenRGB includes its own
   Commander Core driver that will conflict with this plugin. Open OpenRGB, go to
   *Settings* > *Supported Devices*, and **uncheck** both:
   - `Corsair Commander Core`
   - `Corsair Commander Core XT`

   Save the settings and restart OpenRGB.

3. **Install** the plugin using one of these methods:

   **Via OpenRGB UI** — open OpenRGB, go to the *Settings* tab > *Plugins*, click
   *Install Plugin*, and select the downloaded file. OpenRGB will copy it into its
   plugins directory for you.

   **Manual copy** — place the file in the OpenRGB plugins directory yourself:

   | Platform | Plugins directory |
   |---|---|
   | Linux | `~/.config/OpenRGB/plugins/` |
   | Windows | `%APPDATA%\OpenRGB\plugins\` |
   | macOS | `~/.config/OpenRGB/plugins/` |

4. **(Linux only)** Install a udev rule so the device is accessible without root.
   Save the
   [`60-openrgb-corsair-commander-core.rules`](https://raw.githubusercontent.com/Frosthaven/openrgb-plugin-corsair-commander-core/main/udev/60-openrgb-corsair-commander-core.rules)
   file, then run:
   ```bash
   sudo cp 60-openrgb-corsair-commander-core.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```
   If the device isn't detected, you may need to unplug and replug the internal USB
   header or reboot for the new permissions to take effect.

5. **Restart OpenRGB** — the Commander Core device should appear in the device list.

---

## Supported Hardware

This plugin supports all Corsair Commander Core controller variants:

| Controller | USB PID | HID Buffer | Common Products |
|---|---|---|---|
| Commander Core | `0x0C1C` | 96 bytes | Capellix AIOs (early revisions) |
| Commander Core 2 | `0x0C32` | 64 bytes | Capellix XT AIOs (2022+) |
| Commander Core 3 | `0x0C1D` | 96 bytes | |
| Commander Core 4 | `0x0C3C` | 96 bytes | |
| Commander Core 5 | `0x0C3D` | 96 bytes | |
| Commander Core 6 | `0x0C3E` | 96 bytes | |
| Commander Core XT | `0x0C2A` | 384 bytes | Standalone RGB controller |

- USB Vendor ID: `0x1B1C` (Corsair)
- Protocol: USB HID (same command set, different buffer sizes per variant)

The Commander Core ships bundled with Corsair Capellix series AIO liquid coolers
and controls both the pump head RGB and connected fan RGB. The typical setup is:

| Component | LEDs | Channel |
|---|---|---|
| Pump head (Capellix RGB) | 33 | 0 |
| AF120 RGB Elite Fan 1 | 8 | 1 |
| AF120 RGB Elite Fan 2 | 8 | 2 |
| AF120 RGB Elite Fan 3 | 8 | 3 |
| **Total** | **57** | |

## Building from Source

### 1. Clone this repository

```bash
git clone https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core.git
cd openrgb-plugin-corsair-commander-core
```

### 2. Set the path to your OpenRGB source tree

The plugin builds against the OpenRGB headers. Set the `OPENRGB_DIR` environment
variable to the root of your OpenRGB source checkout:

```bash
export OPENRGB_DIR=/path/to/OpenRGB
```

### 3. Install build dependencies

```bash
# Debian/Ubuntu
sudo apt install build-essential qtbase5-dev qtbase5-dev-tools qt5-qmake \
    libqt5svg5-dev libusb-1.0-0-dev libhidapi-dev libmbedtls-dev pkgconf

# Arch Linux
sudo pacman -S qt5-base qt5-svg hidapi libusb mbedtls
```

### 4. Build the plugin

```bash
qmake CorsairCommanderCore.pro
make -j$(nproc)
```

This produces `libOpenRGBCorsairCommanderCorePlugin.so`.

### 5. Install the plugin

```bash
mkdir -p ~/.config/OpenRGB/plugins
cp libOpenRGBCorsairCommanderCorePlugin.so ~/.config/OpenRGB/plugins/
```

### 6. Restart OpenRGB

Launch OpenRGB. The plugin will be loaded automatically and Commander Core devices
should appear in the device list.

## CI / Automated Builds

Every push to `main` triggers a [GitHub Actions workflow](.github/workflows/build.yml)
that builds the plugin for all supported platforms:

| Platform | Architecture | Output |
|---|---|---|
| Linux | x86_64 | `.so` |
| Linux | arm64 | `.so` |
| Linux | armhf | `.so` |
| Windows | x86_64 | `.dll` |
| macOS | arm64 (Apple Silicon) | `.dylib` |
| macOS | x86_64 (Intel) | `.dylib` |

Build artifacts can be downloaded from the
[Actions tab](https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core/actions).

When a version tag is pushed (e.g. `git tag v0.1.0 && git push --tags`), the workflow
also creates a **draft GitHub Release** with all platform binaries attached.

## Testing Without OpenRGB

A standalone Python test script is included so you can verify device communication and
experiment with colors before using the full plugin. It uses direct `/dev/hidraw` I/O and
requires **no pip packages** — just a standard Python 3 install.

### Run

```bash
cd test
sudo python3 test_colors.py
```

> `sudo` is required unless you have installed the udev rules above and re-plugged the
> device.

The script provides an interactive console with these commands:

- `solid <color>` — set all LEDs to a solid color (e.g. `solid #FF0000`)
- `pump <color>` — set pump head only
- `fans <color>` — set all fans only
- `split <pump> <fans>` — pump and fans in different colors
- `rainbow [seconds]` — animated rainbow sweep
- `off` — turn all LEDs off (stays in software mode)
- `info` — display firmware and channel info
- `quit` — return to hardware control mode and exit

Colors can be specified as `#RRGGBB` hex or `R,G,B` decimal.

## Troubleshooting

**Device not found:**
- Make sure Corsair control software is not running
- Check that the Commander Core appears in `lsusb` output (look for VID `1b1c`)
- Verify the udev rules are installed and the device was re-plugged

**Permission denied:**
- Install the udev rules, or run with `sudo`

**Colors not changing:**
- The device may be in hardware mode from a previous session. The plugin
  sends a software-mode command on init, but you may need to power-cycle the device
- Check firmware version — the protocol changed between firmware v1.x and v2.x

## Protocol Reference

The Commander Core protocol used here is a clean-room reimplementation based on protocol
documentation from:

- [OpenLinkHub](https://github.com/jurkovic-nikola/OpenLinkHub) (GPL-3.0) — Go
  implementation (`src/devices/cc/cc.go`) used as protocol reference
- [liquidctl](https://github.com/liquidctl/liquidctl) (GPL-3.0) — Python implementation
  (`docs/developer/protocol/commander_core.md`) used as protocol reference

The plugin icon is from [Lucide Icons](https://lucide.dev/) (ISC license).

> **Note:** This project is an independent MIT-licensed implementation. No code was copied
> from the GPL-licensed reference projects — only the USB HID protocol (command bytes,
> packet layout, endpoint sequences) was studied and reimplemented from scratch.

## License

MIT
