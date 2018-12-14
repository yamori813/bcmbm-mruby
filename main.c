/*
 * Copyright (c) 2018 Hiroki Mori. All rights reserved.
 */

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/irep.h>

#include "xprintf.h"

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


	cfe_init(handle,ept);

	nvram_init(0xa0000000 + 0x1C000000 + 0x003F8000, 0x8000);

	unsigned long init[4]={0x123, 0x234, 0x345, 0x456}, length=4;
	init_by_array(init, length);

	xfunc_out=put;

	print(version);

	cfe_getenv("BOOT_CONSOLE",str,sizeof(str));
	xprintf("BOOT_CONSOLE : %s\n", str);

	cfe_setup_exceptions();
	cfe_irq_init();

	timer_init();

	sizep = 0xa0000000 + 0x1C000000 + 0x40000 + 0x14;
	vmsize = *(sizep + 3) << 24 | *(sizep + 2) << 16 |
	    *(sizep + 1) << 8 | *sizep;
	mrbp = 0xa0000000 + 0x1C000000 + 0x40000 + vmsize;
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
		print("can't find mrb code on flash?n");
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
