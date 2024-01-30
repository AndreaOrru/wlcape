# wlcape

Like https://github.com/alols/xcape, but also works on Wayland.

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

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

## Disclaimer

This project is not an official Google project. It is not supported by
Google and Google specifically disclaims all warranties as to its quality,
merchantability, or fitness for a particular purpose.
