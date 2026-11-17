// SPDX-License-Identifier: GPL-3.0-only
/*
 * r2t2-hw: hardware layer of the DARC R2T2. Replaces the tools r2t2 and r2t2_init of the
 * original system: clock chip (Si5344, I2C), ADC (AD9645) and DAC (AD9717) over bit-banged
 * SPI, reset of the ADC part in the FPGA, preamplifier and attenuator, the rad0 interface.
 *
 *   r2t2-hw init [clock]     everything r2t2_init did (clock 122.88e6, 125e6 or 62.5e6)
 *   r2t2-hw status           what the chips answer
 *   r2t2-hw -g/-k gain  -i/-j att  ...   the options of the old tool (r2t2sdr calls them)
 *
 * The sequences follow the original software by Stefan Goerg (DL2STG),
 * https://github.com/dh1tw/r2t2. Part of https://github.com/DK5DEN/r2t2, see https://afu.tools
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include "tables.h"

#define VERSION "1.0.0"
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* ---- registers in the FPGA and the PS (through /dev/mem) */
#define REG_TX_DIVIDER      0x50000000u
#define REG_TX_FREQ         0x50000004u
#define REG_RX_DIVIDER_BASE 0x50010000u
#define REG_RX_FREQ_BASE    0x50010004u
#define REG_RX_OFFSET       0x00010000u
#define SLCR_FPGA_RST       0xf8000240u
#define PRE_DIVIDER         (16 * 2)

static int errors;
static uint32_t sample_rate = 122880000;

static void fail(const char *what)
{
    fprintf(stderr, "r2t2-hw: %s: %s\n", what, strerror(errno));
    errors++;
}

static int mem(uint32_t addr, uint32_t *val, int write_it)
{
    long ps = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { fail("/dev/mem"); return -1; }
    uint32_t *p = mmap(NULL, ps, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~(ps - 1));
    if (p == MAP_FAILED) { fail("mmap"); close(fd); return -1; }
    if (write_it) p[(addr & (ps - 1)) / 4] = *val;
    else *val = p[(addr & (ps - 1)) / 4];
    munmap(p, ps);
    close(fd);
    return 0;
}
static void set_mem(uint32_t addr, uint32_t val) { mem(addr, &val, 1); }
static uint32_t read_mem(uint32_t addr) { uint32_t v = 0; mem(addr, &v, 0); return v; }

/* ---- GPIO through sysfs, as the old tool did. The chip bases were fixed numbers on the
 * 4.0 kernel (874 FPGA, 906 PS); newer kernels number them differently, so look them up. */
static int base_fpga = -1, base_ps = -1;

static int read_file(const char *path, char *buf, size_t n)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t r = read(fd, buf, n - 1);
    close(fd);
    if (r < 0) return -1;
    buf[r] = 0;
    buf[strcspn(buf, "\n")] = 0;
    return 0;
}

static int gpio_bases(void)
{
    const char *e;
    if ((e = getenv("R2T2_GPIO_FPGA"))) base_fpga = atoi(e);
    if ((e = getenv("R2T2_GPIO_PS"))) base_ps = atoi(e);
    if (base_fpga >= 0 && base_ps >= 0) return 0;
    glob_t g;
    if (glob("/sys/class/gpio/gpiochip*", 0, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++) {
            char p[PATH_MAX], label[64] = "", base[16] = "", n[16] = "";
            snprintf(p, sizeof p, "%s/label", g.gl_pathv[i]); read_file(p, label, sizeof label);
            snprintf(p, sizeof p, "%s/base", g.gl_pathv[i]); read_file(p, base, sizeof base);
            snprintf(p, sizeof p, "%s/ngpio", g.gl_pathv[i]); read_file(p, n, sizeof n);
            if (!base[0]) continue;
            if (strstr(label, "zynq_gpio") || atoi(n) == 118) { if (base_ps < 0) base_ps = atoi(base); }
            else if (strstr(label, "41200000") || atoi(n) == 32) { if (base_fpga < 0) base_fpga = atoi(base); }
        }
        globfree(&g);
    }
    if (base_fpga < 0 || base_ps < 0) {
        fprintf(stderr, "r2t2-hw: GPIO chips not found (FPGA %d, PS %d)\n", base_fpga, base_ps);
        errors++;
        return -1;
    }
    return 0;
}

static int pin_write(int pin, const char *file, const char *val)
{
    char p[64];
    snprintf(p, sizeof p, "/sys/class/gpio/gpio%d/%s", pin, file);
    int fd = open(p, O_WRONLY);
    if (fd < 0) { fail(p); return -1; }
    int r = write(fd, val, strlen(val)) == (ssize_t)strlen(val) ? 0 : -1;
    if (r) fail(p);
    close(fd);
    return r;
}
static void pin_dir(int pin, int out) { pin_write(pin, "direction", out ? "out" : "in"); }
static void pin_set(int pin, int on) { pin_write(pin, "value", on ? "1" : "0"); }
static int pin_get(int pin)
{
    char p[64], v[4] = "";
    snprintf(p, sizeof p, "/sys/class/gpio/gpio%d/value", pin);
    if (read_file(p, v, sizeof v) < 0) { fail(p); return 0; }
    return v[0] == '1';
}
static void pin_export(int pin)
{
    char p[64], v[16];
    snprintf(p, sizeof p, "/sys/class/gpio/gpio%d", pin);
    if (access(p, F_OK) == 0) return;
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) { fail("gpio export"); return; }
    snprintf(v, sizeof v, "%d", pin);
    if (write(fd, v, strlen(v)) < 0 && errno != EBUSY) fail("gpio export");
    close(fd);
}

/* ---- bit-banged SPI */
typedef struct { int sdio, sclk, cs, addr_len; } spi_t;
static spi_t adc, dac, pga1, pga2, att1, att2;
#define TCLK 1

static void spi_out(spi_t s, uint8_t out)
{
    pin_dir(s.sdio, 1);
    for (int i = 0; i < 8; i++, out <<= 1) {
        pin_set(s.sclk, 0);
        pin_set(s.sdio, out & 0x80);
        usleep(TCLK);
        pin_set(s.sclk, 1);
        usleep(TCLK);
    }
    pin_dir(s.sdio, 0);
}
static uint8_t spi_in(spi_t s)
{
    uint8_t b = 0;
    pin_dir(s.sdio, 0);
    for (int i = 0; i < 8; i++) {
        pin_set(s.sclk, 0);
        usleep(TCLK);
        b = (b << 1) | pin_get(s.sdio);
        pin_set(s.sclk, 1);
        usleep(TCLK);
    }
    return b;
}
static void spi_addr(spi_t s, uint8_t rw, uint16_t reg)
{
    if (s.addr_len == 16) { spi_out(s, rw | reg >> 8); spi_out(s, reg & 0xff); }
    else spi_out(s, rw | reg);
}
static uint8_t spi_read(spi_t s, uint16_t reg)
{
    pin_set(s.cs, 0);
    spi_addr(s, 0x80, reg);
    uint8_t v = spi_in(s);
    pin_set(s.cs, 1);
    return v;
}
static void spi_write(spi_t s, uint16_t reg, uint8_t val)
{
    pin_set(s.cs, 0);
    spi_addr(s, 0x00, reg);
    spi_out(s, val);
    pin_set(s.cs, 1);
}
static void spi_byte(spi_t s, uint8_t out)
{
    pin_set(s.sclk, 0);
    pin_set(s.cs, 0);
    spi_out(s, out);
    pin_set(s.cs, 1);
}
static void spi_setup(spi_t s)
{
    pin_export(s.sdio); pin_dir(s.sdio, 0);
    pin_export(s.sclk); pin_dir(s.sclk, 1); pin_set(s.sclk, 1);
    pin_export(s.cs);   pin_dir(s.cs, 1);   pin_set(s.cs, 1);
}
static int spi_init(void)
{
    if (gpio_bases() < 0) return -1;
    int f = base_fpga, p = base_ps;
    adc  = (spi_t){ p + 9, p + 11, p + 10, 16 };
    dac  = (spi_t){ f + 1, f + 0, f + 2, 8 };
    pga1 = (spi_t){ f + 9, f + 10, f + 7, 8 };
    pga2 = (spi_t){ f + 9, f + 10, f + 8, 8 };
    att1 = (spi_t){ f + 6, f + 5, f + 3, 8 };
    att2 = (spi_t){ f + 6, f + 5, f + 4, 8 };
    spi_setup(adc); spi_setup(dac); spi_setup(pga1); spi_setup(pga2); spi_setup(att1); spi_setup(att2);
    return 0;
}

static uint8_t reverse(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}
static void set_gain(int gain, int rx)
{
    uint8_t g = (gain < -10 || gain > 32) ? 0 : gain + 10;
    spi_byte(rx ? pga2 : pga1, 0x80 | reverse(g << 2));
}
static void set_att(int att, int rx)
{
    if (att < 0) att = 0;
    if (att > 31) att = 31;
    spi_byte(rx ? att2 : att1, att << 1);
}

static int adc_id(void) { return spi_read(adc, 0x0001); }
static int init_adc(void)
{
    int id = adc_id();
    if (id != 0x8B) {
        fprintf(stderr, "r2t2-hw: AD9645 not found (chip id %02x)\n", id);
        errors++;
        return -1;
    }
    int grade = spi_read(adc, 0x0002) & 0x70;
    printf("AD9645, %s\n", grade == 0x40 ? "80 MSPS" : grade == 0x50 ? "105 MSPS" : grade == 0x60 ? "125 MSPS" : "unknown grade");
    for (size_t i = 0; i < NELEM(adc_init_data) / 2; i++) {
        spi_write(adc, adc_init_data[i * 2], adc_init_data[i * 2 + 1]);
        if (i == 0) usleep(100000);
    }
    return 0;
}
static void init_dac(void)
{
    printf("AD9717 version %02x\n", spi_read(dac, 31));
    for (size_t i = 0; i < NELEM(dac_init_data) / 2; i++) {
        spi_write(dac, dac_init_data[i * 2], dac_init_data[i * 2 + 1]);
        if (i == 0) usleep(100000);
    }
}

/* ---- Si5344 clock chip on I2C bus 3 (behind the mux), address 0x6b */
#define I2C_BUS_DEFAULT 3
#define I2C_ADDR_SI5344 0x6b

static int i2c_open(void)
{
    char dev[32];
    const char *e = getenv("R2T2_I2C_BUS");
    snprintf(dev, sizeof dev, "/dev/i2c-%d", e ? atoi(e) : I2C_BUS_DEFAULT);
    int fd = open(dev, O_RDWR);
    if (fd < 0) { fail(dev); return -1; }
    if (ioctl(fd, I2C_SLAVE, I2C_ADDR_SI5344) < 0) { fail("I2C_SLAVE"); close(fd); return -1; }
    return fd;
}
static int si_write(int fd, int reg, uint8_t val)
{
    uint8_t page[2] = { 1, reg >> 8 }, data[2] = { reg & 0xff, val };
    if (write(fd, page, 2) != 2 || write(fd, data, 2) != 2) return -1;
    return 0;
}
static int si_read(int fd, int reg)
{
    uint8_t page[2] = { 1, reg >> 8 }, r = reg & 0xff, v;
    if (write(fd, page, 2) != 2 || write(fd, &r, 1) != 1 || read(fd, &v, 1) != 1) return -1;
    return v;
}
static int init_clock(uint32_t rate)
{
    const uint16_t *t;
    size_t n;
    if (rate == 122880000) { t = data_12288mhz; n = NELEM(data_12288mhz); }
    else if (rate == 125000000) { t = data_12500mhz; n = NELEM(data_12500mhz); }
    else if (rate == 62500000) { t = data_6250mhz; n = NELEM(data_6250mhz); }
    else {
        fprintf(stderr, "r2t2-hw: unknown clock %u: usable 125e6, 122.88e6, 62.5e6\n", rate);
        errors++;
        return -1;
    }
    int fd = i2c_open();
    if (fd < 0) return -1;
    int hi = si_read(fd, 3), lo = si_read(fd, 2), rev = si_read(fd, 0);
    if (hi < 0 || lo < 0) { fail("Si5344 does not answer"); close(fd); return -1; }
    printf("clock chip Si%02x%02x rev %d, %u Hz\n", hi, lo, rev, rate);
    for (size_t i = 0; i < n / 2; i++)
        if (si_write(fd, t[i * 2], t[i * 2 + 1]) < 0) {
            fprintf(stderr, "r2t2-hw: Si5344 register %04x: %s\n", t[i * 2], strerror(errno));
            errors++;
            close(fd);
            return -1;
        }
    close(fd);
    return 0;
}

/* ---- rad0: the sample stream needs MTU 65536 and a down/up after the FPGA reset */
static void rad0_cycle(const char *name)
{
    struct ifreq r;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { fail("socket"); return; }
    memset(&r, 0, sizeof r);
    snprintf(r.ifr_name, IFNAMSIZ, "%s", name);
    if (ioctl(s, SIOCGIFFLAGS, &r) < 0) { fail(name); close(s); return; }
    short flags = r.ifr_flags;
    r.ifr_flags = flags & ~IFF_UP;
    if (ioctl(s, SIOCSIFFLAGS, &r) < 0) fail("rad0 down");
    r.ifr_mtu = 65536;
    if (ioctl(s, SIOCSIFMTU, &r) < 0) fail("rad0 mtu");
    r.ifr_flags = flags | IFF_UP;
    if (ioctl(s, SIOCSIFFLAGS, &r) < 0) fail("rad0 up");
    close(s);
}

static uint32_t phase_inc(double f) { return (uint32_t)(pow(2, 30) * f / sample_rate); }
static void set_tx_rate(uint32_t f)
{
    uint32_t div = f ? sample_rate / f / 2 : 0;
    if (div < 125 || div > 3125) { fprintf(stderr, "r2t2-hw: invalid tx rate %u, ignored\n", f); return; }
    set_mem(REG_TX_DIVIDER, div);
}
static void set_rx_rate(int rx, uint32_t f)
{
    uint32_t div = f ? sample_rate / f / PRE_DIVIDER : 0;
    if (div < 4 || div > 8192) { fprintf(stderr, "r2t2-hw: invalid rx rate %u, ignored\n", f); return; }
    set_mem(REG_RX_DIVIDER_BASE + REG_RX_OFFSET * rx, div);
}
static void fpga_adc_reset(void)
{
    set_mem(SLCR_FPGA_RST, 2);
    set_mem(SLCR_FPGA_RST, 0);
}

/* everything /usr/bin/r2t2_init did, in the same order */
static int cmd_init(uint32_t rate)
{
    sample_rate = rate;
    if (init_clock(rate) < 0) return 1;
    if (spi_init() < 0) return 1;
    init_dac();
    init_adc();
    fpga_adc_reset();
    set_mem(REG_TX_FREQ, phase_inc(7e6));
    set_tx_rate(96000);
    set_gain(0, 0); set_gain(0, 1);
    set_att(0, 0);  set_att(0, 1);
    rad0_cycle("rad0");
    if (!errors) printf("r2t2-hw: init ok\n");
    return errors ? 1 : 0;
}

static int cmd_status(void)
{
    int fd = i2c_open();
    if (fd >= 0) {
        printf("clock chip: Si%02x%02x rev %d\n", si_read(fd, 3), si_read(fd, 2), si_read(fd, 0));
        close(fd);
    }
    if (spi_init() == 0) {
        printf("gpio: FPGA base %d, PS base %d\n", base_fpga, base_ps);
        int id = adc_id();
        printf("adc: chip id %02x (%s)\n", id, id == 0x8B ? "AD9645" : "not the AD9645");
        printf("dac: version %02x\n", spi_read(dac, 31));
    }
    char v[32] = "";
    printf("rad0: %s\n", read_file("/sys/class/net/rad0/mtu", v, sizeof v) == 0 ? v : "missing");
    return errors ? 1 : 0;
}

static void usage(void)
{
    puts("r2t2-hw " VERSION ", hardware layer of the R2T2 (https://github.com/DK5DEN/r2t2, https://afu.tools)\n"
         "  r2t2-hw init [clock]   clock chip, ADC/DAC, FPGA reset, gain 0, att 0, rad0 (default 122.88e6)\n"
         "  r2t2-hw status         what the chips answer\n"
         "options of the original tool r2t2:\n"
         "  -s rate  sample clock      -u  init clock chip     -p  init ADC and DAC\n"
         "  -g/-k dB gain ADC 1/2 (-10..32)   -i/-j dB attenuator ADC 1/2 (0..31)\n"
         "  -c rx  -r freq  -a rate    receiver in the old FPGA register layout\n"
         "  -t freq  -b rate           transmitter\n"
         "  -d reg  -l count  -w val   read and write a register");
}
static double num(const char *s)
{
    if (s[0] == '0' && s[1] == 'x') return strtoul(s + 2, NULL, 16);
    if (s[0] == '0' && s[1] == 'b') return strtoul(s + 2, NULL, 2);
    return atof(s);
}

int main(int argc, char *argv[])
{
    if (argc > 1 && !strcmp(argv[1], "init"))
        return cmd_init(argc > 2 ? (uint32_t)num(argv[2]) : 122880000);
    if (argc > 1 && !strcmp(argv[1], "status"))
        return cmd_status();
    if (argc < 2) { usage(); return 1; }
    int c, rx = 1, spi_ok = 0;
    uint32_t reg = 0;
#define SPI() do { if (!spi_ok) { if (spi_init() < 0) return 1; spi_ok = 1; } } while (0)
    while ((c = getopt(argc, argv, "+r:a:b:t:s:pg:k:i:j:ud:w:l:hc:")) != -1) {
        switch (c) {
        case 'c': rx = num(optarg); if (rx < 0 || rx > 8) rx = 1; break;
        case 'r': set_mem(REG_RX_FREQ_BASE + REG_RX_OFFSET * rx, phase_inc(num(optarg))); break;
        case 'a': set_rx_rate(rx, num(optarg)); break;
        case 'b': set_tx_rate(num(optarg)); break;
        case 't': set_mem(REG_TX_FREQ, phase_inc(num(optarg))); break;
        case 's': sample_rate = num(optarg); break;
        case 'u': init_clock(sample_rate); break;
        case 'p': SPI(); init_dac(); init_adc(); break;
        case 'g': SPI(); set_gain(atoi(optarg), 0); break;
        case 'k': SPI(); set_gain(atoi(optarg), 1); break;
        case 'i': SPI(); set_att(atoi(optarg), 0); break;
        case 'j': SPI(); set_att(atoi(optarg), 1); break;
        case 'd': reg = num(optarg); break;
        case 'l':
            if (!reg) { fprintf(stderr, "no reg selected\n"); return 1; }
            for (uint32_t n = 0; n < (uint32_t)num(optarg); n++)
                printf("%08x: %08x\n", reg + n * 4, read_mem(reg + n * 4));
            break;
        case 'w':
            if (!reg) { fprintf(stderr, "no reg selected\n"); return 1; }
            set_mem(reg, num(optarg));
            break;
        default: usage(); return c == 'h' ? 0 : 1;
        }
    }
    return errors ? 1 : 0;
}
