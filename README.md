# r2t2

Everything a DARC R2T2 needs to become a receiver on [afu.tools](https://afu.tools), also for
a device that came without an SD card: base image, hardware layer and installer.

**State 2026-11-17: built and tested in QEMU and against the original tool on a PC. Not tested
on a device yet.**

- [`base-image/`](base-image/) SD card image: boot chain (`boot.bin` of the device), kernel 6.12
  and a minimal Debian, without R2T2 software. Images are attached to the releases.
- [`hw/`](hw/) `r2t2-hw`, the hardware layer: clock chip (Si5344), ADC and DAC, FPGA reset,
  preamplifier and attenuator. It replaces the tools `r2t2` and `r2t2_init` of the original system.
- [`install.sh`](install.sh) the installer: `r2t2-hw` with its service, and afu-remote, which
  carries the receiver engine `r2t2sdr`.

afu-remote itself is generic for all devices and does not live in this repository.

## Install

1. Write the image of the newest `r2t2-base-…` release to an SD card (4 GB or more) with the
   Raspberry Pi Imager or balenaEtcher, insert it, connect the network, switch on.
2. `ssh root@<address>` (host name `r2t2`, also `192.168.1.99`), password `r2t2`. The first
   login asks for a new password. Details: [`base-image/README.md`](base-image/README.md).
3. Run the installer:

       /usr/lib/apt/apt-helper download-file https://raw.githubusercontent.com/DK5DEN/r2t2/main/install.sh /var/tmp/install.sh && sh /var/tmp/install.sh

4. Open the address the installer prints in a browser, enter the code and set the station up.
   After that afu-remote runs as a service and starts the receiver.

Running the installer again updates `r2t2-hw` and repairs the services. afu-remote updates itself.

## Hardware layer

    r2t2-hw init [clock]    everything at the start (service r2t2-hw, clock in /etc/r2t2-hw.conf)
    r2t2-hw status          what the chips answer
    r2t2-hw -g 10 -i 0      the options of the original tool `r2t2` (gain, attenuator, registers)

`hw/build.sh` builds it for the device, `hw/test/compare.sh` runs it and the original tool on a
PC and compares every access to GPIO, I2C and registers.

## License

AGPL-3.0-only with additional terms, see [`LICENSE`](LICENSE) and
[`ADDITIONAL-TERMS.md`](ADDITIONAL-TERMS.md): you may use and change this, if you publish your
changes under the same license and keep the references to the original author (this
repository) and to [afu.tools](https://afu.tools). The kernel parts (`rad0` driver,
device tree) are GPL-2.0. `hw/` is GPL-3.0: its register tables and sequences come from the original
R2T2 software by Stefan Goerg (DL2STG), https://github.com/dh1tw/r2t2. `base-image/boot/boot.bin`
is the unchanged boot file of the device from the same project. License texts: [`LICENSES/`](LICENSES/).
