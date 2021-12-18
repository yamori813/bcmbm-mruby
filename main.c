/*
 * Copyright (c) 2018 Hiroki Mori. All rights reserved.
 */

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/irep.h>

#include "xprintf.h"

typedef struct {
    long long fwi_version;                /* major, minor, eco version */
    long long fwi_totalmem;               /* total installed mem */
    long long fwi_flags;                  /* various flags */
    long long fwi_boardid;                /* board ID */
    long long fwi_bootarea_va;            /* VA of boot area */
    long long fwi_bootarea_pa;            /* PA of boot area */
    long long fwi_bootarea_size;          /* size of boot area */
} cfe_fwinfo_t;

#define	MODULE_UNKNOWN				0
#define	MODULE_RTL8196C				1
#define	MODULE_BCM4712				2
#define	MODULE_RTL8196E				3
#define	MODULE_BCM5350				4
#define	MODULE_BCM5352				5
#define	MODULE_BCM5354				6

extern char version[];

void setbaud(int baud, int port)
{
int clk;

	volatile char* regbase;

	if (port == 0)
		regbase = (volatile char*)0xb8000300;
	else
		regbase = (volatile char*)0xb8000400;

	/* Todo: may be only work BCM5354 */
	clk = 200000000 / 8 / 16 / baud;
	*(volatile char*)(regbase + 3) = 0x80;
	*(volatile char*)regbase = clk & 0xff;
	*(volatile char*)(regbase + 1) = clk >> 8;
	*(volatile char*)(regbase + 3) = 0x03;
}

// put and put2 can't marge one function. It's now work. Why....

void put2(unsigned char c)
{
	volatile char* lsr; // Line status register.
	volatile char* thr; // Transmitter holding register.

	lsr = (volatile char*)0xb8000405;
	thr = (volatile char*)0xb8000400;

	while(((*lsr) & 0x20) == 0) ; // Wait until THR is empty.
 
	*thr = c;
}

void print2(char *ptr)
{
	while(*ptr) {
		put2(*ptr);
		++ptr;
	}
}

void put(unsigned char c)
{
	volatile char* lsr; // Line status register.
	volatile char* thr; // Transmitter holding register.

	lsr = (volatile char*)0xb8000305;
	thr = (volatile char*)0xb8000300;

	while(((*lsr) & 0x20) == 0) ; // Wait until THR is empty.
 
	*thr = c;
}
 
void print(char *ptr)
{
	while(*ptr) {
		put(*ptr);
		++ptr;
	}
}

void cfe_setup_exceptions(void);
void cfe_irq_init(void);
 
int main(unsigned long handle,unsigned long vector,
    unsigned long ept,unsigned long seal)
{
long *lptr;
long clk;
char str[100];
unsigned char *sizep;
int vmsize;
unsigned char *mrbp;
int mrbsize;
unsigned char *mrbbuf;
int bootsize;
cfe_fwinfo_t fwinfo;
int tomem;
int sp;
unsigned char hash[32];
int i;

	cfe_init(handle,ept);

	nvram_init(0xa0000000 + 0x1C000000 + 0x003F8000, 0x8000);

	mt19937ar_init();

	xfunc_out=put;

	print(version);

	cfe_getfwinfo(&fwinfo);
	tomem = fwinfo.fwi_totalmem;
	xprintf("Mem : %x\n", tomem);
	if (tomem > 0x800000) {
		sp = relocsp(tomem - 0x800000);
		xprintf("SP : %x\n", sp);
	}
	cfe_getenv("BOOT_CONSOLE",str,sizeof(str));
	xprintf("BOOT_CONSOLE : %s\n", str);

	cfe_setup_exceptions();
	cfe_irq_init();

	timer_init();

	/* cfe api of FW_GETINFO is wrong bootsize then use hardcode */
	if(isbcm5354())
		bootsize = 0x20000;
	else
		bootsize = 0x40000;

	sizep = 0xa0000000 + 0x1C000000 + bootsize + 0x14;
	vmsize = *(sizep + 3) << 24 | *(sizep + 2) << 16 |
	    *(sizep + 1) << 8 | *sizep;
	mrbp = 0xa0000000 + 0x1C000000 + bootsize + vmsize;
	if (*(mrbp + 0) == 0x52 && *(mrbp + 1) == 0x49 &&
	    *(mrbp + 2) == 0x54 && *(mrbp + 3) == 0x45) {
		mrbsize = *(mrbp + 0x8) << 24 | *(mrbp + 0x9) << 16 |
		    *(mrbp + 0xa) << 8 | *(mrbp + 0xb);
		xprintf("MRB SIZE %d\n", mrbsize);
		mrbbuf = malloc(mrbsize);
		memcpy(mrbbuf, mrbp, mrbsize);
		mksha256(mrbbuf, mrbsize, hash);
		xprintf("MRB SHA256 ");
		for (i = 0; i < 32; ++i)
			xprintf("%02x", hash[i]);
		xprintf("\n");

		mrb_state *mrb;
		mrb = mrb_open();
		mrb_load_irep( mrb, mrbbuf);
		if (mrb->exc) {
			mrb_value exc = mrb_obj_value(mrb->exc);
			mrb_value inspect = mrb_inspect(mrb, exc);
			print(mrb_str_to_cstr(mrb, inspect));
		}
		mrb_close(mrb);
	} else {
		print("can't find mrb code on flash\n");
	}

//	sb_chip_reset();

	return 0;
}

int sr;

cli()
{
	sr = cfe_irq_disable();
}

sti()
{
	cfe_irq_enable(sr);
}

int getarch()
{
int chipid;

	chipid = getchipid();
	if (chipid == 0x4712)
		return MODULE_BCM4712;
	else if (chipid == 0x5350)
		return MODULE_BCM5350;
	else if (chipid == 0x5352)
		return MODULE_BCM5352;
	else if (chipid == 0x5354)
		return MODULE_BCM5354;
	else
		return MODULE_UNKNOWN;
}
