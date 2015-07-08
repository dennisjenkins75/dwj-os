/* bkerndev - Bran's Kernel Development Tutorial
*  By:   Brandon F. (friesenb@gmail.com)
*  Desc: Timer driver
*
*  Notes: No warranty expressed or implied. Use at own risk. */
#include "kernel/kernel.h"

// Total ticks since timer initialized.
unsigned int g_timer_ticks = 0;

// Actual PIT (Programmable Interval Timer) tick rate.
unsigned int g_tick_rate = 0;

uint32	timer_getcount(void)
{
	return g_timer_ticks;
}

uint64	time_get_abs(void)
{
	return g_timer_ticks;	// FIXME: Need better implementation.
}

void	timer_handler(struct regs *r)
{
	char	temp[64];
	int	len;

	g_timer_ticks++;

	len = snprintf (temp, sizeof(temp), "%u Hz: %8u", g_tick_rate, g_timer_ticks);
	con_print(80 - len, 0, 0x1f, len, temp);

#if (DEBUG_TIMER_TICK)
	len = con_settextcolor(14, 5);	// yellow on purple.
	con_putch(0xfe);
	con_set_attr(len);
#endif
}

void	set_timer_phase(int hz)
{
	int divisor = 1193180 / hz;       /* Calculate our divisor */

	disable();

// FIXME: Verify behavor of PIT when speed set to 0.
	g_tick_rate = divisor ? 1193180 / divisor : 0;

	outportb(0x43, 0x36);             /* Set our command byte 0x36 */
	outportb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
	outportb(0x40, divisor >> 8);     /* Set high byte of divisor */

	enable();
}

// Sets up the system clock by installing the timer handler into IRQ0.
void	timer_install(void)
{
	irq_set_handler(0, timer_handler);
}
