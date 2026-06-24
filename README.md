> **Warning**: This is an AI-generated project. The code was authored by an AI assistant
> based on reverse-engineered protocol documentation from the
> [OpenLinkHub](https://github.com/jurkovic-nikola/OpenLinkHub) and
> [liquidctl](https://github.com/liquidctl/liquidctl) projects. It has not been
> extensively tested across all hardware revisions or firmware versions. Use at your own
> risk, and please verify behavior on your specific hardware before relying on it.

# OpenRGB Plugin: Corsair Commander Core

An [OpenRGB](https://openrgb.org/) plugin for the Corsair Commander Core controllers
found in Capellix and Capellix XT series AIO liquid CPU coolers.

It does two things:

1. **RGB** for the pump head and connected fans (replaces OpenRGB's built-in support).
2. **Pump and fan speed control** through a simple **Cooling** tab, including a
   temperature based Auto mode. This works on Capellix XT (firmware 2.x), which other
   Linux tools currently cannot set.

## Install

1. **Download** the plugin for your platform from the
   [Releases](https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core/releases)
   page:

   | Platform | File |
   |---|---|
   | Linux | `libOpenRGBCorsairCommanderCorePlugin.so` |
   | Windows | `OpenRGBCorsairCommanderCorePlugin.dll` |
   | macOS | `libOpenRGBCorsairCommanderCorePlugin.dylib` |

2. **Disable the built-in detector.** OpenRGB has its own Commander Core driver that
   conflicts with this plugin. In OpenRGB go to *Settings > Supported Devices* and
   **uncheck** both `Corsair Commander Core` and `Corsair Commander Core XT`, then save
   and restart OpenRGB.

3. **Install the plugin.** Either use *Settings > Plugins > Install Plugin* in OpenRGB,
   or copy the file into the plugins folder yourself:

   | Platform | Plugins folder |
   |---|---|
   | Linux | `~/.config/OpenRGB/plugins/` |
   | Windows | `%APPDATA%\OpenRGB\plugins\` |
   | macOS | `~/.config/OpenRGB/plugins/` |

4. **(Linux only) Install the udev rule** so the device works without root. Download
   [`60-openrgb-corsair-commander-core.rules`](https://raw.githubusercontent.com/Frosthaven/openrgb-plugin-corsair-commander-core/main/udev/60-openrgb-corsair-commander-core.rules)
   and run:
   ```bash
   sudo cp 60-openrgb-corsair-commander-core.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```
   If the device is not detected afterward, replug the internal USB header or reboot.

5. **Restart OpenRGB.** The cooler appears in the device list, and a **Cooling** tab
   appears at the top.

## RGB

The cooler shows up as a normal OpenRGB device. You can set the pump head and fan colors
the same way as any other device (per LED, zones, effects, etc).

## Cooling tab

The **Cooling** tab lets you pick how the pump and radiator fans run. Your choice is
saved and restored automatically. The default is **Auto**.

| Mode | Pump | Radiator fans |
|---|---|---|
| **Disabled** | not controlled | not controlled |
| **Auto** (default) | quiet at idle, ramps with coolant temp | quiet at idle, ramps with coolant temp |
| **Silent** | ~1130 rpm | ~560 rpm |
| **Quiet** | ~2150 rpm | ~1040 rpm |
| **Balanced** | ~2500 rpm | ~1550 rpm |
| **Performance** | ~2800 rpm | ~2140 rpm |

- **Auto** follows the AIO liquid temperature. It is quiet when idle and speeds up only
  when the coolant warms under load.
- **Disabled** tells the plugin to stop sending speed commands, so the pump and fans run
  on their own or under another tool. (RGB still works.)
- The fans are kept above their stall speed so they never stop unexpectedly, and the pump
  is kept above a safe minimum so coolant always circulates.

### Syncing with other tools

The selected mode is also written to a small text file, so other tools on your system can
read it and match it (for example to drive case fans on a separate controller). The
Cooling tab shows the folder path and has a **Copy folder path** button.

If you want to build a setup where all of your fans follow this same mode, see
**[SYNCED-COOLING.md](SYNCED-COOLING.md)**.

## Troubleshooting

- **Cooler not detected:** make sure Corsair iCUE (or any other Corsair tool) is not
  running, the built-in detector is unchecked (step 2 above), and the udev rule is
  installed (Linux). A replug or reboot helps after installing the rule.
- **Cooling tab is missing:** the device was not detected, so there is nothing to control.
  See above.
- **Pump suddenly loud / device vanished from OpenRGB:** the controller can re-enumerate
  and become invisible to OpenRGB until a replug. See the recovery steps in
  [DEVELOPMENT.md](DEVELOPMENT.md).

## Building and contributing

Build instructions, the protocol notes, and other developer details are in
**[DEVELOPMENT.md](DEVELOPMENT.md)**.

## License

MIT. The plugin icon is from [Lucide Icons](https://lucide.dev/) (ISC license).
