# R2T2 base image

SD card image for the DARC **R2T2** (Zynq Z7020): boot chain, kernel 6.12 and a minimal
Debian 13 (armhf). It contains **no R2T2 software**. Hardware layer, `r2t2sdr` and afu-remote
are installed afterwards by the installer, see the [README](../README.md) of the repository.

**State 2026-11-17: built and started in QEMU only. Not tested on a device yet.**

## Use

1. Write `r2t2-debian-trixie-base-<date>.img.xz` to an SD card (Raspberry Pi Imager or
   balenaEtcher read the packed file directly).
2. Insert the card, connect the network, switch on. The device asks for an address by DHCP
   (host name `r2t2`) and also answers on `192.168.1.99`.
3. `ssh root@<address>`, user `root`, password `r2t2`. The first login asks for a new
   password and closes the connection; log in again with the new one.

Login with a key instead: put your public key as `authorized_keys` (or `authorized_keys.txt`)
on the first partition of the card (FAT, visible under Windows too). It is taken over at every
start. If the key is there before the default password was changed, the default password is
switched off and only the key works; `passwd` sets a password again.

Then install the R2T2 software with the installer, see the [README](../README.md).

## What is in it

- `boot.bin` of the device unchanged (FSBL, bitstream, U-Boot 2014.07), `image.ub` with
  kernel 6.12 and the R2T2 device tree, no modules, no initramfs
- the `rad0` driver (FPGA DMA network interface, DL2STG) built in, ported from linux-4.0-adi
- Debian trixie `minbase` with systemd, networkd (DHCP plus fixed address), resolved,
  timesyncd, OpenSSH and apt; no recommended packages, no docs, no locales
- watchdog, restart after a kernel panic, journal in RAM, `noatime`, the DMA interrupt on the
  second core
- a daily timer that installs Debian security updates only
- the root partition grows to the whole card at the first start

## Layout

- `port/` kernel side: `drivers-net-r2t2/` (driver, `r2t2.c.orig-4.0` is the original),
  `dts/zynq-r2t2.dts`, `config/r2t2.fragment`, `image.its`, `build.sh`
- `image/` `build-image.sh` and the files that go into the image
- `docker/` build containers
- `test/` start of the image in QEMU

## Build

    # kernel source
    mkdir -p kernel build
    curl -L https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.118.tar.xz | tar -xJ -C kernel

    # kernel, device tree, image.ub -> out/
    docker build -t r2t2-kernel-build:trixie docker
    docker run --rm -u $(id -u):$(id -g) -v $PWD:/work r2t2-kernel-build:trixie /work/port/build.sh

    # image -> out-image/   (armhf userland runs through qemu-user binfmt on the build host)
    docker build -t r2t2-image-build:trixie -f docker/Dockerfile.image docker
    docker run --rm -v $PWD:/work -e OWNER=$(id -u):$(id -g) \
        r2t2-image-build:trixie /work/image/build-image.sh

`boot.bin` comes from [`boot/`](boot/), so no original SD card is needed. To use the one of
your own device instead, add `-v /path/to/dir:/geraet1:ro` with `boot/boot.bin` in that directory.

## Test without a device

    docker build -t r2t2-qemu:trixie -f docker/Dockerfile.qemu docker
    ./test/run-qemu.sh            # with a test key
    NOKEY=1 ./test/run-qemu.sh    # default password
    ssh -p 2222 -i test/testkey root@127.0.0.1

QEMU does not know the FPGA: only the kernel start, Debian, the network and SSH can be checked.
`test/qemu-test.dtb` is the real tree with the Ethernet PHY moved to address 7, where QEMU has it.
