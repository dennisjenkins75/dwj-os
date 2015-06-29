/*	kernel/hud.c

	Heads up display.
*/

#include "kernel/kernel.h"

static const char *spinner = "|/-\\";

int	ktask_hud_entry(void *arg)
{
	struct vmm_stats vmm_stats;
	struct task_stats task_stats;
	char	text[80];
	int	len;
	int	sp = 0;

	current->ticks_reload = 1;
	current->ticks_left = 1;

	while (1)
	{
		task_get_stats(&task_stats);
		vmm_get_stats(&vmm_stats);

		len = snprintf (text, sizeof(text), "free: %5d, tasks: %3d %c", vmm_stats.pmm_free_pages, task_stats.total, spinner[sp%4]);
		con_print(80 - len, 1, 0x1f, len, text);

		sp++;
		yield();
	}

	return 0;
}
