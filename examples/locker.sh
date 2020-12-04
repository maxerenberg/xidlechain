#!/bin/sh

# This script uses betterlockscreen (a wrapper around i3lock),
# but any screen locker should work.

# Exit if the locker is already running
if pgrep -U `id -u` i3lock >/dev/null; then
    exit
fi

# Fork to the background so that we don't block
betterlockscreen -l blur &
