#include "cfe/cfe_irq.h"

unsigned int alarm;

unsigned int interval;
unsigned long starttime;
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

	if(isbcm5354()) {
		clk = 240 * 1000 * 1000;
	} else {
		clk = sb_cpu_clock();
		cfe_timer_init(clk);
	}
	xprintf("Clock : %d\n", clk);
	interval = (clk / 2) / 100;

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
	return sys_now()/1000 + starttime;
}
