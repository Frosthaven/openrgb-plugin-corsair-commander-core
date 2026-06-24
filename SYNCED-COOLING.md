# Building a synced cooling setup

The Cooling tab saves your selected mode to a small text file. Any other tool on your
system can read that file and match the mode, so every fan you own can follow one
selection even if the fans live on different controllers.

This guide explains the file and walks through one real example: driving case fans on a
separate Corsair Commander Pro so they follow the same mode as the AIO.

## The mode file

When you pick a mode in the Cooling tab, the plugin writes a single digit to:

```
~/.config/OpenRGB/plugins/settings/CommanderCorePump.conf
```

(The Cooling tab shows this path and has a Copy folder path button.)

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
The small service below reads the mode file and sets those PWM values so the case fans
follow Disabled / Auto / Silent / Quiet / Balanced / Performance too.

> Adapt the PWM numbers to your own fans. Measure yours first: with the service stopped,
> `echo <value> | sudo tee /sys/class/hwmon/hwmonX/pwm1` and read
> `/sys/class/hwmon/hwmonX/fan1_input`. On the fans used here, pwm 0 stopped them, pwm 30
> was about 596 rpm (their floor), and pwm 255 was about 1560 rpm.

### 1. The script

Save as `/usr/local/bin/corsair-case-fans`:

```bash
#!/usr/bin/env bash
# Drive Commander Pro case fans to match the OpenRGB Cooling-tab mode.
set -u

CFG="$HOME/.config/OpenRGB/plugins/settings/CommanderCorePump.conf"
INTERVAL=3
REASSERT_EVERY=20     # re-write even if unchanged every N ticks (~60s)
PWM_FLOOR=40          # never below this (fans stall under ~pwm 30)

find_hwmon() {        # $1 = driver name -> echoes hwmon path
    local h
    for h in /sys/class/hwmon/hwmon*; do
        if [ "$(cat "$h/name" 2>/dev/null)" = "$1" ]; then echo "$h"; return 0; fi
    done
    return 1
}

cpu_temp_c() {
    local h t
    h=$(find_hwmon k10temp) || { echo 50; return; }   # AMD; use coretemp on Intel
    t=$(cat "$h/temp1_input" 2>/dev/null)
    if [ -n "$t" ]; then echo $(( t / 1000 )); else echo 50; fi
}

auto_pwm() {          # $1 = cpu temp C -> stepped curve
    local t=$1
    if   [ "$t" -le 45 ]; then echo 40
    elif [ "$t" -le 55 ]; then echo 80
    elif [ "$t" -le 65 ]; then echo 140
    elif [ "$t" -le 75 ]; then echo 200
    else echo 255; fi
}

last=-1
tick=0
while true; do
    cpro=$(find_hwmon corsaircpro) || { sleep "$INTERVAL"; continue; }

    mode=0
    if [ -r "$CFG" ]; then
        m=$(tr -dc '0-9' < "$CFG" 2>/dev/null | head -c1)
        [ -n "$m" ] && mode=$m
    fi

    # Disabled (5): leave the fans alone for external control.
    if [ "$mode" = "5" ]; then
        last=-1
        sleep "$INTERVAL"
        continue
    fi

    case "$mode" in
        1) pwm=40  ;;   # Silent
        2) pwm=110 ;;   # Quiet
        3) pwm=180 ;;   # Balanced
        4) pwm=255 ;;   # Performance
        *) pwm=$(auto_pwm "$(cpu_temp_c)") ;;   # Auto (mode 0)
    esac

    [ "$pwm" -lt "$PWM_FLOOR" ] && pwm=$PWM_FLOOR

    if [ "$pwm" != "$last" ] || [ "$tick" -ge "$REASSERT_EVERY" ]; then
        for n in 1 2 3 4 5 6; do echo "$pwm" > "$cpro/pwm$n" 2>/dev/null; done
        last=$pwm
        tick=0
    fi

    tick=$((tick + 1))
    sleep "$INTERVAL"
done
```

The mode file lives in your home folder but the PWM files are root owned, so run this as a
system service. Replace `$HOME` with your actual home path in the unit (or set it via the
unit), since a root service does not have your `$HOME`.

### 2. The service

Save as `/etc/systemd/system/corsair-case-fans.service` (set the real home path):

```ini
[Unit]
Description=Corsair Commander Pro case fans follow the OpenRGB Cooling mode
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

Now changing the mode in the Cooling tab moves the AIO pump and radiator fans immediately
and the case fans within a few seconds.

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
