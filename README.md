# xidlechain

This is an idle manager for X. It aims to have an interface similar to that of
[swayidle](https://github.com/swaywm/swayidle).
See the man page, `xidlechain(1)`, for instructions on configuring xidlechain.

See [here](examples/idle.sh) for an example on how to use xidlechain with
an external screen locker.

## Installation

### Compiling from Source

Install dependencies:

* gtk2.0
* Xext (X11 extensions)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optional: man pages)
* g++ >= 8.3.0

On Debian, these can be installed with the following command:

    apt install libgtk2.0-dev libxext-dev scdoc g++

Run these commands:

    make
    make install

## Motivation

[xss-lock](https://bitbucket.org/raymonad/xss-lock) and
[xautolock](https://github.com/l0b0/xautolock) are two popular idle managers
for X. I found it difficult to use these programs for real-life workflows:

* xss-lock does not differentiate between inactivity and suspend signals. This
  means you cannot run a different program for each signal, e.g. we might want
  to gradually dim the screen after inactivity, but not right before we suspend.
* xautolock does not listen to the suspend signal at all, so it is impossible
  to lock the screen right before the systemd suspends.

xidlechain uses timeouts and systemd signals to run programs in a carefully
defined order. Unlike xss-lock and xautolock, which both use the X11 Screensaver
extension, xidlechain uses the
[IDLETIME](https://gitlab.freedesktop.org/xorg/xserver/-/commit/7e2c935920cafadbd87c351f1a3239932864fb90)
system sync counter to detect user inactivity (this approach was used by the
old power manager for Chromium OS). The advantage is that we do not have to
use polling (like xautolock), and we do not need an active screensaver to
know when user activity has resumed (like xss-lock).
