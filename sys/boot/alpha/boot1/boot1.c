/*
 * $FreeBSD$
 * From	$NetBSD: bootxx.c,v 1.4 1997/09/06 14:08:29 drochner Exp $ 
 */

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>

#include <machine/prom.h>
#include <machine/rpb.h>

#define DEBUGxx

extern end[];
int errno;

char *heap = (char*) end;

void
putchar(int c)
{
    if (c == '\n')
	prom_putchar('\r');
    prom_putchar(c);
}

int
getchar()
{
    return prom_getchar();
}

int
ischar()
{
    return prom_poll();
}

void
puts(const char *s)
{
    while (*s)
	putchar(*s++);
}

void *
malloc(size_t size)
{
    char *p = heap;
    size = (size + 7) & ~7;
    heap += size;
    return p;
}

void
free(void * p)
{
}

void
panic(const char *message, ...)
{
    puts(message);
    puts("\r\n");
    halt();
}

int prom_fd = 0;

int
devopen()
{
    prom_return_t ret;
    char devname[64];
    
    if (prom_fd)
	return;

    ret.bits = prom_getenv(PROM_E_BOOTED_DEV, devname, sizeof devname);

    ret.bits = prom_open(devname, ret.u.retval + 1);
    if (ret.u.status)
	panic("devopen: open failed\n");

    prom_fd = ret.u.retval;

    /* XXX read disklabel and setup partition offset */

    return 0;
}

#ifdef DEBUG

void
puthex(u_long v)
{
    int digit;
    char hex[] = "0123456789abcdef";

    if (!v) {
	puts("0");
	return;
    }

    for (digit = 0; v >= (0x10L << digit); digit += 4)
	;

    for (; digit >= 0; digit -= 4)
	putchar(hex[(v >> digit) & 0xf]);
}

#endif

void
devread(char *buf, int block, size_t size)
{
#ifdef DEBUG
    puts("devread(");
    puthex((u_long)buf);
    puts(",");
    puthex(block);
    puts(",");
    puthex(size);
    puts(")\n");
#endif

    prom_read(prom_fd, size, buf, block);
}

static void
devclose()
{
    if (prom_fd) {
	prom_close(prom_fd);
	prom_fd = 0;
    }
}

static void
getfilename(char *filename)
{
    int c;
    char *p = filename;

    puts("Boot: ");

    while ((c = getchar()) != '\r') {
	if (c == '\b' || c == 0177) {
	    if (p > filename) {
		puts("\b \b");
		p--;
	    }
	} else {
	    putchar(c);
	    *p++ = c;
	}
    }
    putchar('\n');
    *p = '\0';
    return;
}

static void
loadfile(char *name, char *addr)
{
    int n;
    char *p;

    puts("Loading ");
    puts(name);
    puts("\n");

    if (openrd(name)) {
	puts("Can't open file ");
	puts(name);
	puts("\n");
	halt();
    }

    p = addr;
    do {
	n = readit(p, 1024);
	p += n;
	twiddle();
    } while (n > 0);

    devclose();
}

static inline u_long rpcc()
{
    u_long v;
    __asm__ __volatile__ ("rpcc %0" : "=r"(v));
    return v & 0xffffffff;
}

int
main()
{
    char *loadaddr = (char*) SECONDARY_LOAD_ADDRESS;
    char *name = "/boot/loader";
    char *p;
    char filename[512];
    void (*entry) __P((void));
    u_long start, freq;
    int	i;

    init_prom_calls();

    start = rpcc();
    freq = ((struct rpb *)HWRPB_ADDR)->rpb_cc_freq;
    while (rpcc() < start + freq) {
	if (ischar()) {
	    getfilename(filename);
	    name = filename;
	    break;
	}
    }

    loadfile(name, loadaddr);

    entry = (void (*)())loadaddr;
    (*entry)();

    return 0;
}
