/*	kernel/demo.c

	Implements various demos that the kernel can do, to test things like threads.
*/

#include "kernel/kernel.h"

#define MAX_TASKS 16

int	ktask_demo_entry(void *arg)
{
	char	temp[80];
	int	demo_row = (int)arg;
	int	x = 0;
	int	y = 2 + demo_row;
	int	w = 30;
	int	l = 0;
	int	val = 0;
	int	pos = 0;
	uint8	attr = ((demo_row & 0x07) * 0x11) | 0x08;
	handle	h;

	ASSERT (are_irqs_enabled());

	if (0 > (int)(h = sem_open("happy", OBJ_KERNEL | OBJ_OPEN_EXISTING, NULL, SEM_MAX_VALUE, 0)))
	{
		PANIC3("sem_open() failed: handle = %p (%s)\n", h, strerror((int)h));
	}

	current->ticks_reload = 1 << demo_row ;

	l = sprintf(temp, "kthread %02d:", current->taskid);
	con_print(x, y, 0x0f, l, temp);
	x += l + 2;

	w = 67 - x;

	for (l = 0; l < w; temp[l++] = 0xb0);
	con_print(x, y, attr, l, temp);

	pos = x;
	val = 0;

	while (1)
	{
		ASSERT (are_irqs_enabled());

		if (0 > (l = obj_wait(h)))
		{
			PANIC4("obj_wait(%p) failed: %d (%s)\n", h, l, strerror(l));
		}

		con_print(x + pos, y, attr, 1, "\xb0");

		val++;
		pos = (val % w);

		con_print(x + pos, y, attr, 1, "\xb1");

		l = sprintf(temp, "%08d", val);
		con_print(x + w + 4, y, 0x07, l, temp);

		if (0 > (l = sem_release(h, 1)))
		{
			PANIC4("sem_release(%p) failed: %d (%s)\n", h, l, strerror(l));
		}
	}

	return 57;
}

void	create_demo_threads(int count)
{
	char		name[32];
	taskid_t	tid[MAX_TASKS];
	int		i;
	handle		h;

	ASSERT(count <= MAX_TASKS);

	if (0 < (int)(h = sem_open("happy", OBJ_KERNEL | OBJ_CREATE_NEW, NULL, SEM_MAX_VALUE, 0)))
	{
		PANIC3("sem_open(create) failed: %p (%s)\n", h, strerror((int)h));
	}

	for (i = 0; i < count; i++)
	{
		sprintf(name, "demo-%d", i);
		tid[i] = task_create(ktask_demo_entry, (void*)(i), name, RUNNABLE);
		ASSERT (tid[i] > 0);
	}

	i = sem_release(h, 1);	// fire up the first thread.
	if (i < 0)
	{
		PANIC4("sem_release(%p) failed: %d (%s)\n", h, i, strerror(i));
	}
}
