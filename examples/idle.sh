#!/bin/sh

# Exit if there is already a running instance
if pgrep -U $(id -u) xidlechain >/dev/null; then
    exit
fi

# Disable the Screensaver and DPMS extensions
xset s off
xset -dpms

# Get the current brightness level
# (using https://github.com/maxerenberg/backlight-dbus)
CUR_BRIGHTNESS=$(backlight-dbus | awk '{print $1}')

# The following command will dim the screen after 20 minutes,
# then suspend the system one minute later. The system will
# lock on suspend and on receiving the Lock signal.
# The brightness will be reset to its original value after
# user activity is detected or the system is woken from sleep.
exec xidlechain -w \
    timeout 1200 "backlight-dbus 5%" \
        resume "backlight-dbus $CUR_BRIGHTNESS" \
    timeout 1260 "systemctl suspend" \
    before-sleep "locker.sh" \
    after-resume "backlight-dbus $CUR_BRIGHTNESS" \
    lock "locker.sh"
