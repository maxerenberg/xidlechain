#!/bin/sh

# Exit if the locker is already running
if pgrep -U `id -u` i3lock >/dev/null; then
    exit
fi

# By default, i3lock forks to the background
i3lock
