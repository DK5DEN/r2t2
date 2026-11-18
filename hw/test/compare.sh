#!/bin/bash
# Compares what r2t2-hw does to the hardware with the original tool, on a PC, without a
# device: both run under shim.so, which records every GPIO, I2C and register access.
#   hw/test/compare.sh /path/to/dh1tw-r2t2/soft/r2t2
# The GPIO bases are set to the numbers the original has built in (874, 906).
set -eu
SRC=${1:?path to soft/r2t2 of https://github.com/dh1tw/r2t2}
cd "$(dirname "$0")"
T=$(mktemp -d)
gcc -O1 -shared -fPIC -o $T/shim.so shim.c -ldl
gcc -O2 -Wall -Wextra -std=gnu99 -o $T/new ../r2t2-hw.c -lm
mkdir $T/o && cp $SRC/*.[ch] $T/o/ && touch $T/o/stropts.h
gcc -O0 -w -Wno-implicit-function-declaration -include sys/ioctl.h -I$T/o -o $T/old $T/o/*.c -lm
export R2T2_GPIO_FPGA=874 R2T2_GPIO_PS=906
# what the chips answer: AD9717 version 0x0a, AD9645 chip id 0x8B, grade 0x60
export SHIM_READS=0a8b60
# Compared is the access to the chips. Left out: pin exports and directions and the idle
# level (the old tool sets up all pins on every call), repeated I2C_SLAVE (the old tool
# opens the bus for every byte).
norm() { grep -v '/export \|/direction \|^I /dev/i2c' $1 || true; }
fail=0
check() { # name, args for both
  local name=$1; shift
  rm -f $T/a.log $T/b.log; touch $T/a.log $T/b.log
  SHIM_LOG=$T/a.log LD_PRELOAD=$T/shim.so $T/old "$@" >/dev/null 2>&1 || true
  SHIM_LOG=$T/b.log LD_PRELOAD=$T/shim.so $T/new "$@" >/dev/null 2>&1 || true
  # the old tool's pin setup: 12 lines "value 1" at the start
  if cmp -s <(norm $T/a.log | tail -n +${SKIPA:-13}) <(norm $T/b.log | tail -n +${SKIP:-13}); then
    echo "same    $name ($(norm $T/b.log | wc -l) accesses)"
  else echo "DIFFER  $name"; diff <(norm $T/a.log | tail -n +${SKIPA:-13}) <(norm $T/b.log | tail -n +${SKIP:-13}) | head -20; fail=1; fi
}
check "ADC and DAC"      -p
SKIPA=13 SKIP=1 check "FPGA reset"       -d 0xf8000240 -w 2
SKIPA=13 SKIP=1 check "tx freq and rate" -s 122.88e6 -t 7e6 -b 96000
# clock chip: the identification read differs in order (the old tool sends the register
# address twice), compared are the register writes after it (12 pin lines and 12 identification lines in the old log, 9 in the new)
for c in 122.88e6 125e6 62.5e6; do SKIPA=25 SKIP=10 check "clock $c" -s $c -u; done
for g in -10 -9 0 13 32; do check "gain $g" -g $g -k $g; done
for a in 0 1 17 31; do check "att $a" -i $a -j $a; done
# receiver frequency: not compared with the old tool, which writes the register of the
# bitstream of June 2016. Expected is 0x53000000 + 4 * rx, as r2t2srv writes it.
rm -f $T/b.log
SHIM_LOG=$T/b.log LD_PRELOAD=$T/shim.so $T/new -c 7 -r 14e6 >/dev/null 2>&1 || true
if [ "$(cat $T/b.log)" = "M 5300001c 074aaaaa" ]; then echo "ok      rx freq"
else echo "DIFFER  rx freq: $(cat $T/b.log)"; fail=1; fi
rm -rf $T
exit $fail
