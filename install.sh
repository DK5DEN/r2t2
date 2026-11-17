#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# Installer for the DARC R2T2 on the base image of this repository
# (https://github.com/DK5DEN/r2t2, https://afu.tools). Run as root on the device:
#
#   /usr/lib/apt/apt-helper download-file https://raw.githubusercontent.com/DK5DEN/r2t2/main/install.sh /var/tmp/install.sh && sh /var/tmp/install.sh
#
# It installs the hardware layer r2t2-hw (clock chip, ADC/DAC, FPGA reset, gain, attenuator)
# with its service, and afu-remote from afu.tools, which carries the receiver engine r2t2sdr.
# Running it again updates r2t2-hw and repairs the services; afu-remote updates itself.
set -eu

HW_VERSION=1.0.0
HW_SHA256=e001035d2b2de071ea395c5e7ebaddfacb76231693100c10dfcbba8e8cb5a976
HW_URL=${HW_URL:-https://github.com/DK5DEN/r2t2/releases/download/hw-$HW_VERSION/r2t2-hw-armhf}
AFU_URL=${AFU_URL:-https://afu.tools/downloads}
DIR=/opt/afu-remote
BENUTZER=funk
UNITS=/etc/systemd/system

sagen() { printf '\n== %s\n' "$*"; }
ende() { printf '\nFehler: %s\n' "$*" >&2; exit 1; }
laden() { # url file
  if command -v curl >/dev/null 2>&1; then curl -fsSL -o "$2" "$1"
  else rm -f "$2"; /usr/lib/apt/apt-helper download-file "$1" "$2" >/dev/null 2>&1; fi
}
summe() { sha256sum "$1" | cut -d' ' -f1; }

[ "$(id -u)" = 0 ] || ende "Bitte als root ausführen."
[ "$(uname -m)" = armv7l ] || ende "Das ist kein R2T2 (Prozessor $(uname -m))."
if ! grep -qi r2t2 /proc/device-tree/model 2>/dev/null && [ -z "${R2T2_TROTZDEM:-}" ]; then
  ende "Das Grundsystem des R2T2 läuft hier nicht (Abbild von https://github.com/DK5DEN/r2t2/releases)."
fi

sagen "Hardware-Schicht r2t2-hw $HW_VERSION"
# /tmp is 64 MB of RAM on the base image: the download goes to the card
T=$(mktemp -d -p /var/tmp)
trap 'rm -rf "$T"' EXIT
if [ ! -x /usr/local/sbin/r2t2-hw ] || [ "$(summe /usr/local/sbin/r2t2-hw)" != "$HW_SHA256" ]; then
  laden "$HW_URL" "$T/r2t2-hw" || ende "r2t2-hw ließ sich nicht laden ($HW_URL)."
  [ "$(summe "$T/r2t2-hw")" = "$HW_SHA256" ] || ende "r2t2-hw: die Prüfsumme stimmt nicht."
  install -m 755 "$T/r2t2-hw" /usr/local/sbin/r2t2-hw
fi
# r2t2sdr sets gain and attenuator through the old tool's name
ln -sf /usr/local/sbin/r2t2-hw /usr/bin/r2t2
[ -f /etc/r2t2-hw.conf ] || printf '# sample clock: 122.88e6 (default), 125e6 or 62.5e6\nCLOCK=122.88e6\n' > /etc/r2t2-hw.conf
cat > $UNITS/r2t2-hw.service <<'EOF'
[Unit]
Description=R2T2 hardware layer: clock chip, ADC/DAC, FPGA reset, rad0
Before=afu-remote.service

[Service]
Type=oneshot
RemainAfterExit=yes
EnvironmentFile=-/etc/r2t2-hw.conf
ExecStart=/usr/local/sbin/r2t2-hw init $CLOCK

[Install]
WantedBy=multi-user.target
EOF
mkdir -p $UNITS/afu-remote.service.d
# afu-remote unpacks itself (about 60 MB) at every start: into /run (RAM), not into the small
# /tmp and not onto the card. The drop-in stays when "afu-remote dienst" rewrites the unit.
printf '[Unit]\nAfter=r2t2-hw.service\nWants=r2t2-hw.service\n\n[Service]\nRuntimeDirectory=afu-remote\nEnvironment=TMPDIR=/run/afu-remote\n' > $UNITS/afu-remote.service.d/r2t2.conf
systemctl daemon-reload
systemctl enable r2t2-hw.service >/dev/null 2>&1
HW_OK=1
systemctl restart r2t2-hw.service || HW_OK=0
journalctl -u r2t2-hw -n 8 --no-pager -o cat 2>/dev/null || true

sagen "afu-remote"
id $BENUTZER >/dev/null 2>&1 || useradd --system --create-home --home-dir /home/$BENUTZER --shell /usr/sbin/nologin $BENUTZER
install -d -o $BENUTZER -g $BENUTZER $DIR
if [ ! -x $DIR/afu-remote ]; then
  laden "$AFU_URL/afu-remote.json" "$T/afu-remote.json" || ende "afu.tools ist nicht erreichbar."
  # the entry "linux-armv7l" of the manifest: file name and checksum
  EINTRAG=$(tr -d '\n' < "$T/afu-remote.json" | sed -n 's/.*"linux-armv7l"[^{]*{\([^}]*\)}.*/\1/p')
  DATEI=$(printf '%s' "$EINTRAG" | sed -n 's/.*"file"[^"]*"\([^"]*\)".*/\1/p')
  SOLL=$(printf '%s' "$EINTRAG" | sed -n 's/.*"sha256"[^"]*"\([0-9a-f]*\)".*/\1/p')
  [ -n "$DATEI" ] && [ -n "$SOLL" ] || ende "Die Liste von afu.tools nennt kein Programm für den R2T2."
  echo "lade $DATEI (etwa 30 MB)"
  laden "$AFU_URL/$DATEI" "$T/afu-remote" || ende "afu-remote ließ sich nicht laden."
  [ "$(summe "$T/afu-remote")" = "$SOLL" ] || ende "afu-remote: die Prüfsumme stimmt nicht."
  install -m 755 -o $BENUTZER -g $BENUTZER "$T/afu-remote" $DIR/afu-remote
fi
echo "afu-remote $(TMPDIR=$T $DIR/afu-remote --version 2>/dev/null || echo '?')"

# Once the setup in the browser has written the config, "afu-remote dienst" turns the
# service into its final form: root part for the receiver engine, station as $BENUTZER.
cat > $UNITS/r2t2-afu-dienst.path <<EOF
[Unit]
Description=R2T2: finish the afu-remote service once it is set up

[Path]
PathExists=$DIR/afu-remote.toml

[Install]
WantedBy=multi-user.target
EOF
cat > $UNITS/r2t2-afu-dienst.service <<EOF
[Unit]
Description=R2T2: afu-remote service with its receiver engine
ConditionPathExists=!/etc/r2t2-afu-dienst.fertig

[Service]
Type=oneshot
RuntimeDirectory=r2t2-afu-dienst
Environment=TMPDIR=/run/r2t2-afu-dienst
ExecStart=$DIR/afu-remote dienst -c $DIR/afu-remote.toml --benutzer $BENUTZER
ExecStartPost=/usr/bin/touch /etc/r2t2-afu-dienst.fertig
ExecStartPost=/usr/bin/systemctl --no-block disable --now r2t2-afu-dienst.path
EOF
if [ ! -f $DIR/afu-remote.toml ]; then
  rm -f /etc/r2t2-afu-dienst.fertig
  cat > $UNITS/afu-remote.service <<EOF
[Unit]
Description=afu-remote station (setup in the browser)
After=network-online.target
Wants=network-online.target

[Service]
User=$BENUTZER
WorkingDirectory=$DIR
ExecStart=$DIR/afu-remote run
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
fi
systemctl daemon-reload
systemctl enable afu-remote.service >/dev/null 2>&1
systemctl restart afu-remote.service
if [ ! -f /etc/r2t2-afu-dienst.fertig ]; then
  systemctl enable r2t2-afu-dienst.path >/dev/null 2>&1
  systemctl restart r2t2-afu-dienst.path
fi

sagen "Fertig"
[ $HW_OK = 1 ] && echo "Hardware: eingerichtet." || echo "Hardware: r2t2-hw init meldet einen Fehler, siehe oben (journalctl -u r2t2-hw)."
if [ -f /etc/r2t2-afu-dienst.fertig ]; then
  echo "afu-remote läuft als Dienst."
else
  echo "Jetzt im Browser einrichten. Adresse und Code:"
  n=0
  while [ $n -lt 30 ]; do
    journalctl -u afu-remote -n 40 --no-pager -o cat 2>/dev/null | grep -q 'Code:' && break
    n=$((n + 1)); sleep 2
  done
  journalctl -u afu-remote -n 40 --no-pager -o cat 2>/dev/null | grep 'Code:' | tail -n 3 \
    || echo "  noch nicht bereit, später: journalctl -u afu-remote | grep Code"
fi
