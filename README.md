# r2t2

Base image and installer for the DARC R2T2.

- [`base-image/`](base-image/) SD card image: boot chain, kernel 6.12 and a minimal Debian,
  without R2T2 software. Images are attached to the releases.

The installer for the R2T2 software (hardware layer, `r2t2sdr`, afu-remote) follows.
afu-remote itself is generic for all devices and lives in a repository of its own.

## License

AGPL-3.0-only with additional terms, see [`LICENSE`](LICENSE) and
[`ADDITIONAL-TERMS.md`](ADDITIONAL-TERMS.md): you may use and change this, if you name this
repository as the source, publish your changes under the same license, and keep the afu.tools
logo and the reference to [afu.tools](https://afu.tools). The kernel parts (`rad0` driver,
device tree) are GPL-2.0.
