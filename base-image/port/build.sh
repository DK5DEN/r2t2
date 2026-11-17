#!/bin/sh
# Builds kernel 6.12 with the rad0 driver, the R2T2 device tree and image.ub.
# Runs inside the container r2t2-kernel-build:trixie, see ../README.md
set -e
V=${KVER:-6.12.118}
P=/work/port; K=/work/kernel/linux-$V; O=/work/out
export ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
cd $K
# 1. driver + device tree into the tree (idempotent)
mkdir -p drivers/net/r2t2
cp $P/drivers-net-r2t2/r2t2.c $P/drivers-net-r2t2/Kconfig $P/drivers-net-r2t2/Makefile drivers/net/r2t2/
grep -q 'r2t2/' drivers/net/Makefile || echo 'obj-$(CONFIG_NET_R2T2) += r2t2/' >> drivers/net/Makefile
grep -q 'r2t2/Kconfig' drivers/net/Kconfig || sed -i 's|^source "drivers/net/ethernet/Kconfig"|source "drivers/net/r2t2/Kconfig"\n\n&|' drivers/net/Kconfig
cp $P/dts/zynq-r2t2.dts arch/arm/boot/dts/xilinx/
grep -q 'zynq-r2t2.dtb' arch/arm/boot/dts/xilinx/Makefile || echo 'dtb-$(CONFIG_ARCH_ZYNQ) += zynq-r2t2.dtb' >> arch/arm/boot/dts/xilinx/Makefile
# 2. config: everything off, then only the fragment
KCONFIG_ALLCONFIG=$P/config/r2t2.fragment make -s allnoconfig
# 3. build
make -j$(nproc) zImage xilinx/zynq-r2t2.dtb > /work/build/kernel-build.log 2>&1 || { tail -40 /work/build/kernel-build.log; exit 1; }
# 4. FIT image in the layout the old U-Boot 2014.07 expects
mkdir -p $O && cd $O
cp $K/arch/arm/boot/zImage $K/arch/arm/boot/dts/xilinx/zynq-r2t2.dtb $K/System.map .
cp $K/.config kernel.config
cp $P/image.its .
mkimage -f image.its image.ub > mkimage.log 2>&1
echo BUILD_OK
