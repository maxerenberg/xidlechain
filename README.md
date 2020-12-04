# xidlechain

This is an idle manager for X. It aims to have the same interface as
[swayidle](https://github.com/swaywm/swayidle).
See the man page, `xidlechain(1)`, for instructions on configuring xidlechain.

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
