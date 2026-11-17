#!/bin/sh
# Builds the R2T2 base image: boot chain + kernel + minimal Debian armhf. No R2T2 software.
# Runs inside r2t2-image-build:trixie, see ../README.md
set -e
SUITE=${SUITE:-trixie}
VER=${VER:-$(date +%Y%m%d)}
F=/work/image/files; O=/work/out-image; R=/tmp/rootfs; B=/tmp/bootfs
rm -rf $R $B; mkdir -p $O $B
PKGS=systemd,systemd-sysv,systemd-resolved,systemd-timesyncd,udev,dbus,openssh-server,iproute2,ca-certificates,e2fsprogs,fdisk,netbase
debootstrap --arch=armhf --variant=minbase --include=$PKGS $SUITE $R http://deb.debian.org/debian

# --- apt/dpkg: nothing recommended, no docs, no locales
cp $F/90r2t2-minimal $R/etc/apt/apt.conf.d/
cp $F/90r2t2-nodoc $R/etc/dpkg/dpkg.cfg.d/
find $R/usr/share/doc -mindepth 2 ! -name copyright -type f -delete 2>/dev/null || true
find $R/usr/share/doc -type d -empty -delete 2>/dev/null || true
rm -rf $R/usr/share/man/* $R/usr/share/info/* $R/usr/share/locale/* $R/usr/share/lintian
rm -f $R/etc/apt/sources.list
SEC="Types: deb
URIs: http://security.debian.org/debian-security
Suites: $SUITE-security
Components: main
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg"
printf 'Types: deb\nURIs: http://deb.debian.org/debian\nSuites: %s %s-updates\nComponents: main\nSigned-By: /usr/share/keyrings/debian-archive-keyring.gpg\n\n%s\n' $SUITE $SUITE "$SEC" > $R/etc/apt/sources.list.d/debian.sources
mkdir -p $R/etc/apt/security.d
echo "$SEC" > $R/etc/apt/security.d/security.sources

# --- base settings
echo r2t2 > $R/etc/hostname
printf '127.0.0.1 localhost\n127.0.1.1 r2t2\n::1 localhost ip6-localhost ip6-loopback\n' > $R/etc/hosts
cp $F/fstab $R/etc/fstab
: > $R/etc/machine-id
rm -f $R/var/lib/dbus/machine-id
# default login root / r2t2; the password has to be changed at the first login
echo 'root:r2t2' | chroot $R chpasswd
chroot $R chage -d 0 root
echo "R2T2 Grundsystem $VER (Debian $SUITE armhf)" > $R/etc/r2t2-base-release

# --- network: DHCP plus the fixed fallback address of the old system
cp $F/10-eth0.network $R/etc/systemd/network/
# keep the kernel names eth0/rad0 (udev would rename eth0 to end0)
ln -sf /dev/null $R/etc/systemd/network/99-default.link
ln -sf /run/systemd/resolve/stub-resolv.conf $R/etc/resolv.conf

# --- logs only in RAM; watchdog fed by systemd, a hung system resets after 10 s
mkdir -p $R/etc/systemd/journald.conf.d $R/etc/systemd/system.conf.d
printf '[Journal]\nStorage=volatile\nRuntimeMaxUse=16M\n' > $R/etc/systemd/journald.conf.d/r2t2.conf
printf '[Manager]\nRuntimeWatchdogSec=10s\nRebootWatchdogSec=2min\n' > $R/etc/systemd/system.conf.d/r2t2-watchdog.conf

# --- ssh: key from the boot partition or the default password
mkdir -p $R/etc/ssh/sshd_config.d
printf 'PasswordAuthentication yes\nKbdInteractiveAuthentication no\nPermitRootLogin yes\n' > $R/etc/ssh/sshd_config.d/r2t2.conf
rm -f $R/etc/ssh/ssh_host_*

# --- own helpers and units
install -m 755 $F/r2t2-base-firstboot $F/r2t2-base-sshkey $F/r2t2-base-irq $F/r2t2-base-security-update $R/usr/local/sbin/
install -m 644 $F/units/* $R/etc/systemd/system/
chroot $R systemctl enable systemd-networkd systemd-resolved systemd-timesyncd ssh \
  r2t2-base-firstboot r2t2-base-sshkey r2t2-base-irq r2t2-base-security-update.timer
chroot $R systemctl disable apt-daily.timer apt-daily-upgrade.timer || true

# --- clean
chroot $R apt-get clean
rm -rf $R/var/lib/apt/lists/* $R/var/cache/apt/*.bin $R/var/log/*.log $R/var/log/apt $R/tmp/* $R/root/.bash_history
chroot $R dpkg-query -W -f '${Package} ${Version} ${Installed-Size}\n' > $O/packages.txt

# --- boot partition content
cp /geraet1/boot/boot.bin $B/boot.bin
cp /work/out/image.ub $B/image.ub
sed 's/$/\r/' $F/LIESMICH.txt > $B/LIESMICH.txt

# --- assemble: MBR, p1 FAT32 100 MiB, p2 ext4 just large enough
USED=$(du -sm $R | cut -f1)
ROOTMB=$(( USED * 13 / 10 + 48 ))
BOOTMB=100
IMG=$O/r2t2-debian-$SUITE-base-$VER.img
rm -f $IMG $IMG.xz /tmp/boot.fat /tmp/root.ext4
truncate -s ${BOOTMB}M /tmp/boot.fat
mkfs.vfat -F 32 -n R2T2BOOT /tmp/boot.fat >/dev/null
mcopy -i /tmp/boot.fat $B/boot.bin $B/image.ub $B/LIESMICH.txt ::
truncate -s ${ROOTMB}M /tmp/root.ext4
mkfs.ext4 -q -L r2t2root -d $R /tmp/root.ext4
truncate -s $(( 1 + BOOTMB + ROOTMB ))M $IMG
printf 'label: dos\nstart=2048, size=%s, type=c, bootable\nstart=%s, type=83\n' $(( BOOTMB * 2048 )) $(( (1 + BOOTMB) * 2048 )) | sfdisk -q $IMG
dd if=/tmp/boot.fat of=$IMG bs=1M seek=1 conv=notrunc status=none
dd if=/tmp/root.ext4 of=$IMG bs=1M seek=$(( 1 + BOOTMB )) conv=notrunc status=none
xz -T0 -9 -k $IMG
( cd $O && sha256sum $(basename $IMG.xz) > $(basename $IMG.xz).sha256 )
echo "rootfs used ${USED} MB, root partition ${ROOTMB} MB"
chown -R ${OWNER:-0:0} $O
ls -l $O
echo IMAGE_OK
