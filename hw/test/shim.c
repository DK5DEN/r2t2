// SPDX-License-Identifier: AGPL-3.0-only
/* Test only: LD_PRELOAD shim that records what a program does to the R2T2 hardware (GPIO
 * sysfs, I2C, /dev/mem) instead of doing it, so r2t2-hw can be compared with the original
 * tool on a PC. Log: $SHIM_LOG. GPIO reads return the bits of $SHIM_READS (hex), then 1. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAXFD 256
static char *names[MAXFD];
static FILE *lg;
static struct { void *p; unsigned long addr; } maps[16];
static unsigned bitpos;

static void out(const char *fmt, ...)
{
    if (!lg) lg = fopen(getenv("SHIM_LOG") ? getenv("SHIM_LOG") : "/tmp/shim.log", "a");
    va_list ap; va_start(ap, fmt); vfprintf(lg, fmt, ap); va_end(ap); fflush(lg);
}
static int ours(const char *p)
{
    return !strncmp(p, "/sys/class/gpio/", 16) || !strncmp(p, "/dev/i2c-", 9) || !strcmp(p, "/dev/mem");
}
static int known(int fd) { return fd >= 0 && fd < MAXFD && names[fd]; }
static char next_bit(void)
{
    const char *h = getenv("SHIM_READS");
    unsigned n = bitpos++;
    if (!h || n / 4 >= strlen(h)) return '1';
    char c[2] = { h[n / 4], 0 };
    return (strtoul(c, NULL, 16) >> (3 - n % 4)) & 1 ? '1' : '0';
}
int open(const char *path, int flags, ...)
{
    static int (*real)(const char *, int, ...);
    if (!real) real = dlsym(RTLD_NEXT, "open");
    va_list ap; va_start(ap, flags); int mode = va_arg(ap, int); va_end(ap);
    if (!ours(path)) return real(path, flags, mode);
    int fd = real("/dev/null", O_RDWR);
    if (fd >= 0 && fd < MAXFD) names[fd] = strdup(path);
    return fd;
}
ssize_t write(int fd, const void *buf, size_t n)
{
    static ssize_t (*real)(int, const void *, size_t);
    if (!real) real = dlsym(RTLD_NEXT, "write");
    if (!known(fd)) return real(fd, buf, n);
    if (!strncmp(names[fd], "/dev/i2c-", 9)) {
        out("W %s", names[fd]);
        for (size_t i = 0; i < n; i++) out(" %02x", ((const uint8_t *)buf)[i]);
        out("\n");
    } else
        out("W %s %.*s\n", names[fd], (int)n, (const char *)buf);
    return n;
}
ssize_t read(int fd, void *buf, size_t n)
{
    static ssize_t (*real)(int, void *, size_t);
    if (!real) real = dlsym(RTLD_NEXT, "read");
    if (!known(fd)) return real(fd, buf, n);
    out("R %s\n", names[fd]);
    *(char *)buf = !strncmp(names[fd], "/dev/i2c-", 9) ? 0x44 : next_bit();
    return 1;
}
int ioctl(int fd, unsigned long req, ...)
{
    static int (*real)(int, unsigned long, ...);
    if (!real) real = dlsym(RTLD_NEXT, "ioctl");
    va_list ap; va_start(ap, req); void *arg = va_arg(ap, void *); va_end(ap);
    if (!known(fd)) return real(fd, req, arg);
    out("I %s %lx %lx\n", names[fd], req, (unsigned long)arg);
    return 0;
}
int close(int fd)
{
    static int (*real)(int);
    if (!real) real = dlsym(RTLD_NEXT, "close");
    if (known(fd)) names[fd] = NULL;
    return real(fd);
}
void *mmap(void *a, size_t len, int prot, int flags, int fd, off_t off)
{
    static void *(*real)(void *, size_t, int, int, int, off_t);
    if (!real) real = dlsym(RTLD_NEXT, "mmap");
    if (!known(fd) || strcmp(names[fd], "/dev/mem")) return real(a, len, prot, flags, fd, off);
    uint32_t *p = real(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (size_t i = 0; i < len / 4; i++) p[i] = 0xA5A5A5A5u;
    for (int i = 0; i < 16; i++) if (!maps[i].p) { maps[i].p = p; maps[i].addr = (unsigned long)off; break; }
    return p;
}
int munmap(void *p, size_t len)
{
    static int (*real)(void *, size_t);
    if (!real) real = dlsym(RTLD_NEXT, "munmap");
    for (int i = 0; i < 16; i++) if (p && maps[i].p == p) {
        for (size_t k = 0; k < len / 4; k++)
            if (((uint32_t *)p)[k] != 0xA5A5A5A5u) out("M %08lx %08x\n", maps[i].addr + 4 * k, ((uint32_t *)p)[k]);
        maps[i].p = NULL;
    }
    return real(p, len);
}
