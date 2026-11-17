#!/bin/sh
# Writes tables.h from the original R2T2 software (https://github.com/dh1tw/r2t2, soft/r2t2).
#   hw/gen-tables.sh /path/to/r2t2/soft/r2t2 > hw/tables.h
S=${1:?path to soft/r2t2}
cat <<'EOF'
/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Register tables of the R2T2: Si5344 clock chip, AD9645 ADC, AD9717 DAC.
 * Taken unchanged from the original R2T2 software by Stefan Goerg (DL2STG),
 * https://github.com/dh1tw/r2t2 (soft/r2t2/si5344_init.c and spi.c), GPL-3.0.
 * Written by hw/gen-tables.sh, do not edit by hand.
 */
EOF
for t in data_12288mhz data_12500mhz data_6250mhz; do
  awk "/^uint16_t $t\\[\\]/,/^};/" $S/si5344_init.c | sed 's/^uint16_t/static const uint16_t/'; echo
done
awk '/^uint8_t dac_init_data\[\]/,/^};/' $S/spi.c | sed 's/^uint8_t/static const uint8_t/'; echo
awk '/^uint16_t adc_init_data\[\]/,/^};/' $S/spi.c | sed 's/^uint16_t/static const uint16_t/'
