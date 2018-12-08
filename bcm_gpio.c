/*
 * Copyright (c) 2018 Hiroki Mori. All rights reserved.
 */

#define	CHIPC_GPIOIN	0xb8000060
#define	CHIPC_GPIOOUT	0xb8000064
#define	CHIPC_GPIOOUTEN	0xb8000068
#define	CHIPC_GPIOCTRL	0xb800006c
#define	CHIPC_GPIOPOL	0xb8000070
#define	CHIPC_GPIOINTM	0xb8000074

unsigned long gpio_getctl()
{
unsigned long *lptr;

	lptr = (unsigned long *)CHIPC_GPIOCTRL;

	return *lptr;
}

void gpio_setctl(unsigned long val)
{
unsigned long *lptr;

	lptr = (unsigned long *)CHIPC_GPIOCTRL;

	*lptr = val;
}

unsigned long gpio_getdir()
{
unsigned long *lptr;

	lptr = (unsigned long *)CHIPC_GPIOOUTEN;

	return *lptr;
}

void gpio_setdir(unsigned long val)
{
unsigned long *lptr;

	lptr = (unsigned long *)CHIPC_GPIOOUTEN;

	*lptr = val;
}

unsigned long gpio_getdat()
{
unsigned long *lptr;

	lptr = (unsigned long *)CHIPC_GPIOIN;

	return *lptr;
}

void gpio_setdat(unsigned long val)
{
unsigned long *lptr;

	lptr = (unsigned long *)CHIPC_GPIOOUT;

	*lptr = val;
}
