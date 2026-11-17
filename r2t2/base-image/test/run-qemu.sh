#!/bin/sh
# Starts the built image in QEMU (no FPGA there: only kernel, Debian, network and SSH can be checked).
# NOKEY=1 leaves the SSH key away (login with the default password).
# qemu-test.dtb = the real tree with the PHY moved to address 7, where QEMU has it. Only for this test.
cd "$(dirname "$0")/.."
IMG=$(ls out-image/*.img | tail -1)
[ -f test/testkey ] || ssh-keygen -q -t ed25519 -N "" -f test/testkey
cp $IMG test/sd.img && truncate -s 1G test/sd.img
[ -n "$NOKEY" ] || docker run --rm -u $(id -u):$(id -g) -v $PWD/test:/t r2t2-image-build:trixie mcopy -o -i /t/sd.img@@1M /t/testkey.pub ::authorized_keys.txt
docker rm -f r2t2-qemu-test >/dev/null 2>&1
docker run -d --rm --name r2t2-qemu-test -p 127.0.0.1:2222:2222 -v $PWD:/work r2t2-qemu:trixie timeout ${T:-900} qemu-system-arm -M xilinx-zynq-a9 -m 896M -display none -serial null -serial file:/work/test/qemu-image-boot.log -kernel /work/out/zImage -dtb /work/test/qemu-test.dtb -append "console=ttyPS0,115200 root=/dev/mmcblk0p2 rw rootfstype=ext4 net.ifnames=1" -drive if=sd,format=raw,file=/work/test/sd.img -nic user,hostfwd=tcp::2222-:22
