![C/C++ CI](https://github.com/maxerenberg/xidlechain/workflows/C/C++%20CI/badge.svg)

# xidlechain

This is an idle manager for X.
See the man page [xidlechain(1)](xidlechain.1.scd) for configuration instructions.

## Installation

### Compiling from Source

Install dependencies:

* gtk3
* libgudev
* Xext (X11 extensions)
* pulseaudio
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optional: man pages)
* g++ >= 8.3.0

On Debian, these can be installed with the following command:

    apt install libgtk-3-dev libgudev-1.0-dev libxext-dev libpulse-dev scdoc g++

On Fedora:

    dnf install gtk3-devel libgudev-devel libXext-devel pulseaudio-libs-devel scdoc g++

Once the dependencies have been installed, run the following:

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
  to lock the screen right before the system suspends.

xidlechain uses timeouts and systemd signals to run programs in a carefully
defined order. Unlike xss-lock and xautolock, which both use the X11 Screensaver
extension, xidlechain uses the
[IDLETIME](https://gitlab.freedesktop.org/xorg/xserver/-/commit/7e2c935920cafadbd87c351f1a3239932864fb90)
system sync counter to detect user inactivity (this approach was used by the
old power manager for Chromium OS). The advantage is that we do not have to
use polling (like xautolock), and we do not need an active screensaver to
know when user activity has resumed (like xss-lock).
