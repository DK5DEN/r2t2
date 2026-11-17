#!/bin/sh
# Builds r2t2-hw for the device (armhf, static) in the container of the kernel build:
#   docker build -t r2t2-kernel-build:trixie base-image/docker    (once)
#   hw/build.sh          -> hw/r2t2-hw, its checksum belongs into install.sh (HW_SHA256)
cd "$(dirname "$0")"
docker run --rm -u $(id -u):$(id -g) -v "$PWD":/hw -w /hw r2t2-kernel-build:trixie \
  arm-linux-gnueabihf-gcc -O2 -Wall -Wextra -std=gnu99 -static -s -o r2t2-hw r2t2-hw.c -lm
sha256sum r2t2-hw
