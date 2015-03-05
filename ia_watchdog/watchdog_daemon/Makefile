CC =  ../../../../prebuilt/linux-x86/toolchain/i686-linux-glibc2.7-4.4.3/bin/i686-linux-gcc
CFLAGS = -m32
LDFLAGS = -m32

all:	watchdogd

watchdogd:	watchdogd.o
	$(CC) $(LDFLAGS) -o watchdogd watchdogd.o

watchdogd.o:	watchdogd.c watchdogd.h
	$(CC) $(CFLAGS) -c watchdogd.c

clean:
	rm -f watchdogd watchdogd.o
