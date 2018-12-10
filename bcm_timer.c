#include "cfe/cfe_irq.h"

unsigned int alarm;

#define INTERVAL 20000000

unsigned int interval;
unsigned int starttime;
static volatile unsigned int jiffies = 0;

static void
timer_isr(void *arg)
{
	net_poll();

	alarm += interval;
	_setalarm(alarm);
	++jiffies;
}

timer_init()
{
int clk;

	clk = sb_cpu_clock();
	xprintf("Clock : %d\n", clk);
	cfe_timer_init(clk);
	interval = 0x30000;

	cfe_request_irq(5, timer_isr, 0, CFE_IRQ_FLAGS_SHARED, 0);
	cfe_enable_irq(5);
	alarm = _getticks() + interval;
	_setalarm(alarm);
}

int sys_now()
{
	return jiffies * 10;
}

void reset_counter()
{
	jiffies = 0;
}

delay_ms(int ms)
{
	cfe_usleep(ms * 1000);
}

unsigned long
time(unsigned long *t)
{
	return 1544224252;
}
