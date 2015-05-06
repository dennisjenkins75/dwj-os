/*	kernel/reaper.c

	Implements 'reaper' task, which disposes of the dead tasks.
*/

#include "kernel/kernel/kernel.h"

int	ktask_reaper_entry(void *arg)
{
	current->ticks_left = 1;
	current->ticks_reload = 1;

	while (1)
	{
		yield();

		// FIXME: relace this with "yield()" when I get that coded.
//		__asm__ __volatile__ ("hlt");
	}

	return 0;	// this task should never exit.
}
