/*	kernel/ktasks/startup.c		*/

#include "kernel/kernel/kernel.h"

int	ktask_startup_entry(void *ptr)
{
	con_cls();

	task_create(ktask_hud_entry, NULL, "[hud]", RUNNABLE);

	reaper_taskid = task_create(ktask_reaper_entry, NULL, "[reaper]", RUNNABLE);

	create_demo_threads(8);

	return 0;
}
