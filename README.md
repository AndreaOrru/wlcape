# wlcape

Helps remapping Caps Lock to Escape when pressed alone, or Ctrl otherwise.

Like https://github.com/alols/xcape, but works on both X11 and Wayland.

## Installation

You'll need the following dependencies on Debian-based systems:

```sh
sudo apt install build-essential libudev-dev
```

To build and install:
```sh
make
sudo make install
```

By default, this will install the binary in `/usr/local/bin` and the systemd unit in `/usr/lib/systemd/system`.

## Usage

### 1. Remap Caps Lock to Ctrl

First, configure your desktop environment to remap Caps Lock to Ctrl.
- **GNOME:** Tweaks > Keyboard & Mouse > Additional Layout Options > Caps Lock behavior > Make Caps Lock an additional Ctrl
- **KDE:** System Settings > Hardware > Input Devices > Keyboard > Advanced > Caps Lock behavior > Make Caps Lock an additional Ctrl
- **Sway:** https://github.com/swaywm/sway/wiki#keyboard-layout

### 2. Enable systemd service

```sh
sudo systemcl enable --now wlcape.service
```

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

## Disclaimer

This project is not an official Google project. It is not supported by
Google and Google specifically disclaims all warranties as to its quality,
merchantability, or fitness for a particular purpose.
