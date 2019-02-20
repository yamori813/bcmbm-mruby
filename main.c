/*
 * Copyright (c) 2018 Hiroki Mori. All rights reserved.
 */

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/irep.h>

#include "xprintf.h"

#define	MODULE_UNKNOWN				0
#define	MODULE_RTL8196C				1
#define	MODULE_BCM4712				2
#define	MODULE_RTL8196E				3
#define	MODULE_BCM5350				4
#define	MODULE_BCM5352				5
#define	MODULE_BCM5354				6

extern char version[];

void put(unsigned char c)
{
	volatile char* lsr = (volatile char*)0xb8000305; // Line status register.
	volatile char* thr = (volatile char*)0xb8000300; // Transmitter holding register.
 
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


	cfe_init(handle,ept);

	nvram_init(0xa0000000 + 0x1C000000 + 0x003F8000, 0x8000);

	mt19937ar_init();

	xfunc_out=put;

	print(version);

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
		mrbsize = *(mrbp + 0xa) << 24 | *(mrbp + 0xb) << 16 |
		    *(mrbp + 0xc) << 8 | *(mrbp + 0xd);
		mrbbuf = malloc(mrbsize);
		memcpy(mrbbuf, mrbp, mrbsize);

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
