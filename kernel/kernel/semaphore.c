/*	kernel/kernel/semaphore.c

	Implements the semaphore object type.
*/

#include "kernel/kernel.h"

void	_sem_unsignal(struct hnode *hnode, struct task *task);

struct onode_ops semaphore_ops =
{
	.type = OBJ_SEMAPHORE,
	.unsignal = _sem_unsignal,
	.close = NULL
};

struct	sem_data
{
	int	current;
	int	max;
};

#define SEM_EXTRA_BYTES (sizeof(struct sem_data))

handle	sem_open(const char *name, int flags, int *disposition, int max, int initial)
{
	handle	h;
	int	d;	// disposition (did it open a new one?)

	if ((initial < 0) || (max < 1))
	{
		return (handle)-EINVAL;
	}

	h = _obj_open(name, flags, &semaphore_ops, SEM_EXTRA_BYTES, &d);

	if (disposition)
	{
		*disposition = d;
	}

	if ((int)h < 0)
	{
		return h;
	}

	if (!d)		// _obj_open created a new object.
	{
		struct sem_data *sem;
		struct hnode *hnode = _obj_get(h);
		ASSERT(hnode);

		sem = (struct sem_data*)&(hnode->onode->extra[0]);

		sem->current = initial;
		sem->max = max;
		hnode->onode->signalled = sem->current > 0;

		_obj_release(hnode);
	}

	return h;
}

// Increase semaphore's value by count (unless count would put semaphore above max).
// Release one thread for every increase above zero.
int	sem_release(handle h, int count)
{
	struct hnode *hnode = NULL;
	struct sem_data *sem = NULL;

	if (count < 0)
	{
		return -EINVAL;
	}

	if (NULL == (hnode = _obj_get(h)))
	{
		return -EINVAL;
	}

	sem = (struct sem_data*)&(hnode->onode->extra[0]);

	if (sem->current + count > sem->max)
	{
		_obj_release(hnode);
		return -EINVAL;
	}

	sem->current += _obj_wake(hnode, count);
	hnode->onode->signalled = sem->current > 0;
	_obj_release(hnode);

	return 0;
}

void	_sem_unsignal(struct hnode *hnode, struct task *task)
{
	struct sem_data *sem = NULL;

	ASSERT(hnode);
	ASSERT(task);
	ASSERT(spinlock_is_locked(&task->lock));
	ASSERT(spinlock_is_locked(&hnode->onode->lock));

	sem = (struct sem_data*)&(hnode->onode->extra[0]);

	ASSERT(sem->current > 0);

	sem->current--;
	hnode->onode->signalled = sem->current > 0;
}
