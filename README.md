# r2t2

Base image and installer for the DARC R2T2.

- [`base-image/`](base-image/) SD card image: boot chain, kernel 6.12 and a minimal Debian,
  without R2T2 software. Images are attached to the releases.

The installer for the R2T2 software (hardware layer, `r2t2sdr`, afu-remote) follows.
afu-remote itself is generic for all devices and lives in a repository of its own.
