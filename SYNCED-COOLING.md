# Building a synced cooling setup

The Commander Core Cooling tab saves your selected mode to a small text file. Any other tool on
your system can read that file and match the mode, so every fan you own can follow one
selection even if the fans live on different controllers.

This guide explains the file and walks through one real example: driving case fans on a
separate Corsair Commander Pro so they follow the same mode as the AIO.

## The mode file

When you pick a mode in the Commander Core Cooling tab, the plugin writes a single digit to:

```
~/.config/OpenRGB/plugins/settings/CommanderCorePump.conf
```

(The tab shows this path and has a Copy folder path button.)

The digit is the mode:

| Value | Mode |
|---|---|
| `0` | Auto |
| `1` | Silent |
| `2` | Quiet |
| `3` | Balanced |
| `4` | Performance |
| `5` | Disabled |

A companion tool just needs to read this file every few seconds and set its own fans to
match. That is the entire contract.

## Example: case fans on a Corsair Commander Pro

A Commander Pro is supported by the Linux kernel `corsair-cpro` driver, which exposes the
fans as standard hwmon PWM files (`/sys/class/hwmon/hwmonX/pwm1..6`, values 0 to 255).
The service below reads the mode file and sets those PWM values so the case fans follow
Disabled / Auto / Silent / Quiet / Balanced / Performance too.

> Adapt the PWM numbers to your own fans. Measure yours first: with the service stopped,
> `echo <value> | sudo tee /sys/class/hwmon/hwmonX/pwm1` and read
> `/sys/class/hwmon/hwmonX/fan1_input`. On the fans used here, pwm 0 stopped them, pwm 30
> was about 596 rpm (their floor), and pwm 255 was about 1560 rpm.

### 1. The script

Save as `/usr/local/bin/corsair-case-fans`:

```bash
#!/usr/bin/env bash
#
# corsair-case-fans
# -----------------
# Drives Corsair Commander Pro case fans (via the kernel corsair-cpro hwmon PWM
# files) so they follow the mode picked in the OpenRGB "Commander Core Cooling" tab.
# The plugin writes the selected mode (0-5) to a small config file; this service
# reads that same file every few seconds and sets the fan PWM to match.
#
#   Modes:  0 Auto   1 Silent   2 Quiet   3 Balanced   4 Performance   5 Disabled
#
# Runs as root because the hwmon pwm* files are owned by root.
#
set -u

# --- Settings ---------------------------------------------------------------

# Mode file written by the OpenRGB plugin. A root systemd service does NOT
# inherit your $HOME, so set HOME= in the unit file (see step 2 below). If that
# line is missing, fail with a clear message instead of dying on an unbound
# variable and then crash-looping silently every RestartSec seconds.
if [ -z "${HOME:-}" ]; then
    echo "corsair-case-fans: \$HOME is not set. Add 'Environment=HOME=/home/<user>'" \
         "to the [Service] section of the unit file (see step 2)." >&2
    exit 1
fi
CFG="$HOME/.config/OpenRGB/plugins/settings/CommanderCorePump.conf"

INTERVAL=3            # seconds between checks
REASSERT_EVERY=20     # re-write the PWM even if unchanged every N loops (~60s),
                      # in case something else nudged the fans
PWM_FLOOR=40          # lowest PWM we will ever set (0-255). Below ~30 these fans
                      # stall; 40 keeps them spinning quietly (~600 rpm here)

# --- Helpers ----------------------------------------------------------------

# Find a hwmon folder by its driver name. hwmon numbers can change between
# reboots, so always look the device up by name instead of hard-coding hwmonX.
find_hwmon() {                        # $1 = driver name -> prints the hwmon path
    local h
    for h in /sys/class/hwmon/hwmon*; do
        if [ "$(cat "$h/name" 2>/dev/null)" = "$1" ]; then echo "$h"; return 0; fi
    done
    return 1
}

# Read the CPU temperature in whole degrees C. Pick the sensor for YOUR CPU by
# leaving one line active and commenting the other out:
#   AMD Ryzen / Threadripper -> k10temp
#   Intel Core               -> coretemp
cpu_temp_c() {
    local h t
    h=$(find_hwmon k10temp)      # AMD Ryzen / Threadripper
    # h=$(find_hwmon coretemp)   # Intel Core
    if [ -z "${h:-}" ]; then echo 50; return; fi   # sensor missing -> assume 50C
    t=$(cat "$h/temp1_input" 2>/dev/null)           # value is in millidegrees C
    if [ -n "$t" ]; then echo $(( t / 1000 )); else echo 50; fi
}

# Auto-mode fan curve: CPU temperature -> PWM (0-255), as simple steps.
# Tune to taste; higher numbers are louder but cooler.
auto_pwm() {                          # $1 = cpu temp C
    local t=$1
    if   [ "$t" -le 45 ]; then echo 40     # idle: quiet floor
    elif [ "$t" -le 55 ]; then echo 80
    elif [ "$t" -le 65 ]; then echo 140
    elif [ "$t" -le 75 ]; then echo 200
    else echo 255; fi                      # hot: full speed
}

# --- Main loop --------------------------------------------------------------

last=-1               # last PWM written (-1 = unknown, forces the first write)
tick=0                # loop counter for the periodic re-assert
while true; do
    # Locate the Commander Pro. If it is not present yet, wait and retry.
    cpro=$(find_hwmon corsaircpro) || { sleep "$INTERVAL"; continue; }

    # Read the selected mode (first digit in the config file). Default to Auto.
    mode=0
    if [ -r "$CFG" ]; then
        m=$(tr -dc '0-9' < "$CFG" 2>/dev/null | head -c1)
        [ -n "$m" ] && mode=$m
    fi

    # Disabled (5): do not touch the fans; leave them to run externally.
    if [ "$mode" = "5" ]; then
        last=-1                       # force a re-assert when re-enabled
        sleep "$INTERVAL"
        continue
    fi

    # Map the mode to a target PWM. Auto uses the temperature curve above.
    case "$mode" in
        1) pwm=40  ;;   # Silent      (~600 rpm here)
        2) pwm=110 ;;   # Quiet       (~850 rpm)
        3) pwm=180 ;;   # Balanced    (~1250 rpm)
        4) pwm=255 ;;   # Performance (~1560 rpm)
        *) pwm=$(auto_pwm "$(cpu_temp_c)") ;;   # Auto (mode 0)
    esac

    # Never below the stall floor.
    [ "$pwm" -lt "$PWM_FLOOR" ] && pwm=$PWM_FLOOR

    # Only write when the value changed, plus a periodic re-assert. Keeping the
    # device traffic low lets it play nicely with OpenRGB's RGB on the same hub.
    if [ "$pwm" != "$last" ] || [ "$tick" -ge "$REASSERT_EVERY" ]; then
        for n in 1 2 3 4 5 6; do echo "$pwm" > "$cpro/pwm$n" 2>/dev/null; done
        last=$pwm
        tick=0
    fi

    tick=$((tick + 1))
    sleep "$INTERVAL"
done
```

### 2. The service

Save as `/etc/systemd/system/corsair-case-fans.service` (set the real home path):

```ini
[Unit]
Description=Corsair Commander Pro case fans follow the OpenRGB Commander Core Cooling mode
After=multi-user.target

[Service]
Type=simple
Environment=HOME=/home/YOUR_USERNAME
ExecStart=/usr/local/bin/corsair-case-fans
Restart=on-failure
RestartSec=5
Nice=5

[Install]
WantedBy=multi-user.target
```

### 3. Install

```bash
sudo install -m 755 corsair-case-fans /usr/local/bin/corsair-case-fans
sudo install -m 644 corsair-case-fans.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now corsair-case-fans.service
```

Now changing the mode in the Commander Core Cooling tab moves the AIO pump and radiator fans
immediately and the case fans within a few seconds.

### Notes on coexistence

The kernel `corsair-cpro` driver sets fan PWM while OpenRGB drives the Commander Pro RGB
over hidraw. These coexist in practice. PWM is only written when the value changes (plus a
periodic refresh), which keeps traffic to the device low.

### Remove

```bash
sudo systemctl disable --now corsair-case-fans.service
sudo rm /usr/local/bin/corsair-case-fans /etc/systemd/system/corsair-case-fans.service
sudo systemctl daemon-reload
```
