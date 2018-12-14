/*
 * Copyright (c) 2018 Hiroki Mori. All rights reserved.
 */

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/irep.h>

#include "hoge.c"

#include "xprintf.h"

extern char _end[];
extern char _fbss[];

extern char version[];

void put(unsigned char c)
{
	volatile char* lsr = (volatile char*)0xb8000305; // Line status register.
	volatile char* thr = (volatile char*)0xb8000300; // Transmitter holding register.
 
	while(((*lsr) & 0x20) == 0) ; // Wait until THR is empty.
 
	*thr = c;
}
 
mrb_value myputs(mrb_state *mrb, mrb_value self){
        char *ptr;
        mrb_value val;
        mrb_get_args(mrb, "S", &val);
        for (ptr = RSTRING_PTR(val); *ptr != '\0'; ++ptr) {
                put(*ptr);
        }
        put('\r');
        put('\n');
        return mrb_nil_value();
}

void print(char *ptr)
{
	while(*ptr) {
		put(*ptr);
		++ptr;
	}
}

void cfe_attach_init(void);
void cfe_irq_init(void);
void mactest(void);
void cfe_setup_exceptions(void);
 
int main(unsigned long handle,unsigned long vector,
    unsigned long ept,unsigned long seal)
{
long *lptr;
long clk;
char str[100];

        /* bss clear */
/*
        for (lptr = (long *)_fbss; lptr < (long *)_end; ++lptr) {
                 *lptr = 0;
        }
*/

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

        mrb_state *mrb;
        mrb = mrb_open();
        mrb_define_method(mrb, mrb->object_class,"myputs", myputs,
            MRB_ARGS_REQ(1));
        mrb_load_irep( mrb, bytecode);
        mrb_close(mrb);

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
