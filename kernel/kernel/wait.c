/*	kernel/kernel/wait.c

	Implements "obj_wait()" and "obj_wait_many()".
*/

#include "kernel/kernel/kernel.h"

#define T() printf ("line: %d\n", __LINE__)
//#define T()

void	_obj_dump_wait_node(const char *header, const struct wait_node *wn)
{
	uint8	attr = con_set_attr(0x1f);
	printf("%s: wn(%p) t=%p(id:%d, wc:%d, wl:%p)\n", header, wn, wn->task, wn->task->taskid, wn->task->wait_count, wn->task->wait_list);
	printf("    obj=%p (wc:%d, wl:%p)\n", wn->onode, wn->onode->wait_count, wn->onode->wait_list);
	printf("    tn=%p tp=%p\n", wn->task_next, wn->task_prev);
	printf("    on=%p op=%p\n", wn->obj_next, wn->obj_prev);
	con_set_attr(attr);
}

// Internal function.  Disconnects "wn" from the list of tasks and list of objects
// that it links to.  If the "wn->task" or "wn->onode" point to this "wn", update
// them too.  Leaves "wn->task" and "wn->onode" pointers intact (the objects that
// they point to will be modified).
void	_obj_wn_detach(struct wait_node *wn)
{
	ASSERT_HEAP(wn);
	ASSERT_HEAP(wn->onode);
	ASSERT_HEAP(wn->task);

	ASSERT(spinlock_is_locked(&wn->onode->lock));
	ASSERT(spinlock_is_locked(&wn->task->lock));
	ASSERT(spinlock_is_locked(&task_list_lock));

//	_obj_dump_wait_node("detach", wn);

	ASSERT_HEAP(wn->task_next);
	ASSERT_HEAP(wn->task_prev);
	ASSERT_HEAP(wn->obj_next);
	ASSERT_HEAP(wn->obj_prev);

	ASSERT(wn->task_next->task_prev == wn);
	ASSERT(wn->task_prev->task_next == wn);
	ASSERT(wn->obj_next->obj_prev == wn);
	ASSERT(wn->obj_prev->obj_next == wn);

//	_obj_dump_wait_node("before", wn);

	wn->onode->wait_count--;
	wn->task->wait_count--;
	ASSERT(wn->onode->wait_count >= 0);
	ASSERT(wn->task->wait_count >= 0);

// If "wn" was head object for this task's wait-list, make next object the new head.
	if (wn->task->wait_list == wn)
	{
		wn->task->wait_list = (wn->obj_next != wn) ? wn->obj_next : NULL;
	}

// If "wn" was head task for this object's wait-list, make next task the new head.
	if (wn->onode->wait_list == wn)
	{
		wn->onode->wait_list = (wn->task_next != wn) ? wn->task_next : NULL;
	}

// Remove "wn" from lists in both dimensions.
	wn->task_next->task_prev = wn->task_prev;
	wn->task_prev->task_next = wn->task_next;
	wn->obj_next->obj_prev = wn->obj_prev;
	wn->obj_prev->obj_next = wn->obj_next;

// Clear these so that accidentally dereferencing them will force a crash.
	wn->obj_next = wn->obj_prev = wn->task_next = wn->task_prev = NULL;

//	_obj_dump_wait_node("after", wn);

// Sanity checking.
	if (!wn->onode->wait_count) ASSERT(wn->onode->wait_list == NULL);
	if (wn->onode->wait_count) ASSERT(wn->onode->wait_list != NULL);
	if (!wn->task->wait_count) ASSERT(wn->task->wait_list == NULL);
	if (wn->task->wait_count) ASSERT(wn->task->wait_list != NULL);
}

// Internal function that wakes the next "count" tasks blocking on the object.
// Returns left-over count value.
int	_obj_wake(struct hnode *hnode, int count)
{
	struct wait_node	*wn = NULL;
	uint64			t_entry = time_get_abs();

	ASSERT(hnode);
	ASSERT((count == -1) || (count > 0));
	ASSERT(spinlock_is_locked(&hnode->onode->lock));

	spinlock_acquire(&task_list_lock);

//printf("\n_obj_wake(%p, %d) (wc: %d)\n", hnode, count, hnode->onode->wait_count);

	while (count && hnode->onode->wait_list)
	{
// Extract "wn" from the list.  We need this for later use.
		wn = hnode->onode->wait_list;

// Any checks against the task require that we lock it first.
		spinlock_acquire(&wn->task->lock);

// Remove the "wn" from the ADT.
		_obj_wn_detach(wn);
//printf("back from detach\n");

// One of three things just happened:
// 1) wait_count went to zero.  We wake the task.
// 2) wait_count is non-zero but wn->task->wait_all is false, so we wake the task
//	AND unlink all of the other "wn" chained to this task.
// 3) wait_count is non-zero and wn->task->wait_all is true, so we continue to sleep

// Should we wake the task?
		if (!wn->task->wait_count || !wn->task->wait_all)
		{
//printf("making task (%p) RUNNABLE\n", wn->task);
			wn->task->state = RUNNABLE;	// All this fuss to switch one flag...
			wn->task->wait_time = t_entry - wn->began;

// If the task was waiting on additional objects, free those wait_nodes now.
// FIXME: Need to test to see if the compiler will over-optimize this...
//printf("Checking for additional objects held by this task.\n");
			while (wn->task->wait_list)
			{
				struct wait_node *temp = wn->task->wait_list;
				_obj_wn_detach(temp);
				kfree(temp);
			}
//printf("while loop of death is done.\n");

			ASSERT(!wn->task->wait_count);
		}

		spinlock_release(&wn->task->lock);
		kfree(wn);

		if (count != -1)
		{
			count--;
		}
	}

	spinlock_release(&task_list_lock);

	return count;
}

// Internal function to add a wait_node between the task and a handle.
void	_obj_add_wait(struct task *task, struct hnode *hnode, struct wait_node *wn)
{
	ASSERT(task);
	ASSERT(hnode);
	ASSERT(wn);

	ASSERT(spinlock_is_locked(&task->lock));
	ASSERT(spinlock_is_locked(&hnode->onode->lock));

	wn->task = task;
	wn->onode = hnode->onode;
	wn->began = time_get_abs();	// FIXME: This should be passed in.

// Link wait_node to it self (pre-requisite for proper linked-list inserts below).
	wn->task_next = wn->task_prev = wn->obj_next = wn->obj_prev = wn;

	if (!task->wait_list)
	{
		task->wait_list = wn;
	}

	if (!hnode->onode->wait_list)
	{
		hnode->onode->wait_list = wn;
	}

	task->wait_count++;
	hnode->onode->wait_count++;

	wn->task_next = hnode->onode->wait_list;
	wn->task_prev = hnode->onode->wait_list->task_prev;
	hnode->onode->wait_list->task_prev->task_next = wn;
	hnode->onode->wait_list->task_prev = wn;

	wn->obj_next = task->wait_list;
	wn->obj_prev = task->wait_list->obj_prev;
	task->wait_list->obj_prev->obj_next = wn;
	task->wait_list->obj_prev = wn;

	ASSERT(task->wait_list->task_next->task_prev == task->wait_list);
	ASSERT(task->wait_list->task_prev->task_next == task->wait_list);

	ASSERT(hnode->onode->wait_list->task_next->task_prev == hnode->onode->wait_list);
	ASSERT(hnode->onode->wait_list->task_prev->task_next == hnode->onode->wait_list);

	ASSERT(wn->task_next->task_prev == wn);
	ASSERT(wn->task_prev->task_next == wn);

	ASSERT(wn->obj_next->obj_prev == wn);
	ASSERT(wn->obj_prev->obj_next == wn);
}

/*	obj_wait(handle h);

	Causes calling task to sleep until the object is signaled.
	If the object is already signaled upon entry, the task
	won't sleep.  Upon exit, the signaled object may change state
	(like a mutex), but may not (like a zombie task).

	Returns 0 if the thread wakes up normally, or <0 on error.
*/

int	obj_wait(handle h)
{
	struct hnode *hnode = NULL;
	struct wait_node *wn = NULL;

	ASSERT(current);	// not allowed to block if scheduler is not active.
	ASSERT(!current->wait_list);
	ASSERT(!current->wait_count);

	if (NULL == (wn = (struct wait_node *)kmalloc(sizeof(*wn), HEAP_FAILOK)))
	{
		return -ENOMEM;
	}

	if (NULL == (hnode = _obj_get(h)))	// automatically locks object.
	{
		return -EINVAL;
	}

	spinlock_acquire(&current->lock);

//PANIC2("first wait.  signalled = %d\n", hnode->onode->signalled);

// If the object is already signalled, we won't wait on it.  Will need to expand
// this code when we implement "wait_many()".
	if (hnode->onode->signalled)
	{
		// Need to "unsignal" it.
		if (hnode->onode->ops->unsignal)
		{
			hnode->onode->ops->unsignal(hnode, current);
		}

		kfree(wn);
		spinlock_release(&current->lock);
		_obj_release(hnode);

		return 0;
	}


	current->wait_all = 0;
	_obj_add_wait(current, hnode, wn);

// Put the task to sleep.
	current->state = WAITING;

	spinlock_release(&current->lock);
	_obj_release(hnode);

	schedule();

//printf("task %d returned from wait(%p)\n", current->taskid, h);

// FIXME: Determine if we return "0" or -EWAIT_ABANDONED.

	return 0;
}
