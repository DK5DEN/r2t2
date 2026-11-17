CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=gnu99

r2t2-hw: r2t2-hw.c tables.h
	$(CC) $(CFLAGS) -o $@ r2t2-hw.c -lm

clean:
	rm -f r2t2-hw
