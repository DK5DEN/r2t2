FROM debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
    crossbuild-essential-armhf build-essential bc bison flex libssl-dev libelf-dev \
    u-boot-tools device-tree-compiler kmod cpio xz-utils rsync ca-certificates curl python3 git \
    && rm -rf /var/lib/apt/lists/*
