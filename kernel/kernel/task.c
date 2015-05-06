/*	kernel/task.c	*/

#include "kernel/kernel.h"

static taskid_t		next_taskid = 0;
static spinlock		next_taskid_lock = INIT_SPINLOCK("next_taskid");

spinlock		task_list_lock = INIT_SPINLOCK("task_list");
struct task		*task_list = NULL;

struct task		*current = NULL;
static struct task	*idle_task = NULL;

taskid_t		reaper_taskid = 0;

extern struct tss_t	tss;	// FIXME: This belongs in a header file.

taskid_t		gen_taskid(void)
{
	taskid_t	taskid = 0;
	struct task	*task = NULL;

	spinlock_acquire(&next_taskid_lock);
	spinlock_acquire(&task_list_lock);

	taskid = next_taskid;

// Although theoretically this loop could run forever, there is a practical limit on the
// number of actual tasks (task structs must fit on kernel's heap).  So I don't expect
// to exhaust all possible taskids.
// Note: on initial invocation, "task_list" will be NULL.
redo:
	for (task = task_list; task; task = task->task_next)
	{
		if (task->taskid == taskid)
		{
			taskid++;
			goto redo;
		}

		if (task->task_next == task_list)
		{
			break;
		}
	}

	next_taskid = taskid + 1;

	spinlock_release(&task_list_lock);
	spinlock_release(&next_taskid_lock);

	return taskid;
}

void		task_entry(void *task_ptr) __attribute__ ((__noreturn__));

void		task_entry(void *task_ptr)
{
	struct task *ptask = (struct task*)task_ptr;

//	printf("task_entry: %d (%s)\n", ptask->taskid, ptask->name ? ptask->name : "??");

// NOTE: When a task is VERY first ran, the scheduler has a lock on the task_list (which also
// blocks local IRQs.  We need to release this lock for proper operation.
	ASSERT(!are_irqs_enabled());
	spinlock_release(&task_list_lock);
	ASSERT(are_irqs_enabled());

	ptask->exit_code = ptask->entry(ptask->arg);
	ptask->state = ZOMBIE;
	schedule();

	PANIC2("zombies ate my kernel! (task ptr = %p)\n", task_ptr);
}

taskid_t	task_create(int (*entry_point)(void *arg), void *arg, const char *name, enum task_state init_state)
{
	struct task	*task = NULL;
	taskid_t	ret = 0;
	uint32		*kstack = NULL;

	if (!entry_point || !name)
	{
		return -EINVAL;
	}

	if ((init_state != PAUSED) && (init_state != RUNNABLE))
	{
		return -EINVAL;
	}

	if (NULL == (task = (struct task*)kmalloc(sizeof(struct task), HEAP_FAILOK)))
	{
		return -ENOMEM;
	}

	if (NULL == (kstack = (uint32*)kmalloc(TASK_KSTACK_SIZE, HEAP_FAILOK)))
	{
		ret = -ENOMEM;
		goto error;
	}

	memset(task, 0, sizeof(task));
	task->kstack = kstack;
	task->kstack_size = TASK_KSTACK_SIZE;
	task->state = init_state;
	task->taskid = gen_taskid();
	task->exit_code = 0;
	task->ring = 0;
	task->entry = entry_point;
	task->arg = arg;
	task->ticks_left = task->ticks_reload = DEFAULT_THREAD_QUANTUM;
	strcpy_s(task->name, sizeof(task->name), name);
	spinlock_init(&task->lock, task->name);
	task->wait_count = 0;
	task->wait_list = NULL;

	kstack += (task->kstack_size / sizeof(uint32));	// Top of stack.

// Build a stack that looks like what "switch_stack" will expect.
// However, when "switch_stack" returns, we want EIP to point to
// our wrapper entry point.
	*(--kstack) = (uint32)task;			// argument to "task_entry()"
	*(--kstack) = 0;				// address "task_entry()" returns to.
	*(--kstack) = (uint32)task_entry;		// eip (what switch_task will 'ret' to).
	*(--kstack) = 0;				// ebp
	*(--kstack) = 0;				// ebx (callee saved register)
	*(--kstack) = 0;				// esi (callee saved register)
	*(--kstack) = 0;				// edi (callee saved register)

//printf("%s: task->esp = %p (* = %p), task->kstack = %p\n", name, kstack, *kstack, task->kstack);
	task->proc_esp = (uint32)kstack;
	task->proc_cr3 = get_cr3();			// FIXME: This won't work if thread is created when user thread was interrupted.

// Insert task into queue at end.
	spinlock_acquire(&task_list_lock);
	{
		task->task_next = task_list;
		task->task_prev = task_list->task_prev;
		task_list->task_prev->task_next = task;
		task_list->task_prev = task;

		ASSERT(task->task_next->task_prev == task);
		ASSERT(task->task_prev->task_next == task);
		ASSERT(task_list->task_prev->task_next == task_list);
		ASSERT(task_list->task_next->task_prev == task_list);
	}
	spinlock_release(&task_list_lock);

	return task->taskid;

error:
	if (task->kstack) kfree(task->kstack);
	if (task) kfree(task);
	return ret;
}

/*  Called during kernel initialization.  Prior to calling this function there are
    no "tasks" executing as understood by the scheduler.  "create_initial_task"
    converts the current kernel thread into a task.  This task should get PID 0,
    and will become the future "idle" task.  It does not need a seperate kernel stack.
*/
void	scheduler_init(void)
{
	struct task		*task = NULL;

	corehelp.task_list_ptr_ptr = (uint32)&task_list;
	corehelp.task_current_ptr_ptr = (uint32)&current;

	task = (struct task*)kmalloc(sizeof(struct task), 0);
	task->state = RUNNING;
	task->taskid = gen_taskid();
	task->exit_code = 0;
	task->ring = 0;
	task->kstack = (void*)0xcbcbcbcb;	// FIXME: ???
	task->kstack_size = (uint32)&_kernel_stack_start + (uint32)&_kernel_stack_size;
	task->proc_esp = 0xbbbbbbbb;		// FIXME: ???
	task->proc_cr3 = get_cr3();
	task->ticks_left = 1;
	task->ticks_reload = 1;
	strcpy_s(task->name, sizeof(task->name), "[idle]");
	spinlock_init(&task->lock, task->name);
	task->wait_count = 0;
	task->wait_list = NULL;

	ASSERT(task->taskid == 0);

	spinlock_acquire(&task_list_lock);
	{
		task->task_next = task->task_prev = task_list = task;
		current = task;
		idle_task = task;
	}
	spinlock_release(&task_list_lock);
}

// Called to put the current task to sleep and select the next runnable task.
// Normally called after timer interrupt acknowledged, so must deal with
// re-entrnecy issues.
void	schedule(void)
{
	struct task	*new_task = NULL;
	uint32		*old_esp_ptr = NULL;

	if ((current->state == RUNNING) || (current->state == RUNNABLE))
	{
		if (current->ticks_left > 0)
		{
			current->ticks_left--;
			return;
		}

		current->ticks_left = current->ticks_reload;
	}

	spinlock_acquire(&task_list_lock);

	if (current->state == RUNNING)
	{
		current->state = RUNNABLE;
	}

// Find the next runnable task, skipping the idle task.  O(n).
	for (new_task = current->task_next; new_task != current; new_task = new_task->task_next)
	{
//printf("scanning task (iter: %p, ptr:%p, id:%d state:%d)\n", iter, new_task, new_task->taskid, new_task->state);

		if (new_task->taskid == 0)
		{
			continue;	// skip idle task for now.
		}

		if (new_task->state == RUNNABLE)
		{
			break;
		}
	}

// If we went through the entire list and found nothing, choose the idle task if the current one
// is not runnable, otherwise, just leave the current task alone.
	if (new_task == current)
	{
		if (current->state == RUNNABLE)
		{
//printf("refusing to switch to current task. (%d)\n", task_list_lock.lock);
			spinlock_release(&task_list_lock);
			return;
		}

//printf("choosing idle task\n");
		new_task = idle_task;
	}

//printf("switching from %s to %s\n", current->name, new_task->name);
	new_task->state = RUNNING;

// Switch memory spaces.  This will flush the TLB entirely.
	if (get_cr3() != new_task->proc_cr3)
	{
		set_cr3(new_task->proc_cr3);
	}

// Switch stacks.
	tss.esp0 = (uint32)new_task->kstack + (uint32)(new_task->kstack_size);

	old_esp_ptr = &(current->proc_esp);
	current = new_task;

	{
		char temp[82];
		int len = sprintf(temp, "cur %02d %s", current->taskid, current->name);
		while (len < 30)
		{
			temp[len++] = ' ';
		}
		con_print(0, 0, 0x0b, len, temp);
	}

//printf("Calling switch_stacks(%p,%p) (*%p = %p)\n", new_task->proc_esp, old_esp_ptr, old_esp_ptr, *old_esp_ptr);
//Halt();
	switch_stacks(new_task->proc_esp, old_esp_ptr);

//printf("returned from task switch.\n");

	spinlock_release(&task_list_lock);
}

// Does not move task to different queues, just alters the state.
// Meant for pausing and unpausing threads.
int	task_set_state(taskid_t taskid, enum task_state state)
{
	struct task *temp = NULL;
	int ret = 0;

	spinlock_acquire(&task_list_lock);

	for (temp = task_list; ; temp = temp->task_next)
	{
		if (temp->taskid == taskid)
		{
			break;
		}

		if (temp->task_next == task_list)
		{
			break;
		}
	}

	if (temp->taskid != taskid)
	{
		spinlock_release(&task_list_lock);
		return -ENOENT;
	}

	spinlock_acquire(&temp->lock);

	if ((state == RUNNABLE && temp->state == PAUSED) ||
	    (state == PAUSED && (temp->state == RUNNABLE || temp->state == RUNNING)))
	{
		temp->state = state;
		ret = 0;
	}
	else
	{
		ret = -EINVAL;
	}

	spinlock_release(&temp->lock);
	spinlock_release(&task_list_lock);

	return ret;
}

// WARNING: returned struct could disappear once task is reaped.
struct task*	task_get_ptr(taskid_t taskid)
{
	struct task *temp = NULL;

	spinlock_acquire(&task_list_lock);

	for (temp = task_list; ; temp = temp->task_next)
	{
		if (temp->taskid == taskid)
		{
			break;
		}

		if (temp->task_next == task_list)
		{
			break;
		}
	}

	if (temp->taskid != taskid)
	{
		temp = NULL;
	}

	spinlock_release(&task_list_lock);
	return temp;
}

int	task_get_stats(struct task_stats *stats)
{
	struct task *temp = NULL;

	memset(stats, 0, sizeof(stats));
	spinlock_acquire(&task_list_lock);

	for (temp = task_list; ; temp = temp->task_next)
	{
		stats->total++;
		if ((temp->state >= 0) && (temp->state < TASK_STATES))
		{
			stats->states[temp->state]++;
		}

		if (temp->task_next == task_list)
		{
			break;
		}
	}

	spinlock_release(&task_list_lock);
	return 0;
}

void	yield(void)
{
	schedule();
}

static const char *task_state_names[TASK_STATES] =
{
	"runnable", "running", "waiting", "paused", "zombie"
};

void	task_dump_list(void)
{
	struct task *temp = NULL;

	for (temp = task_list; ; temp = temp->task_next)
	{
		printf("task %d (%s) is %d (%s) (wc: %d)\n",
			temp->taskid, temp->name, temp->state, task_state_names[temp->state], temp->wait_count);

		if (temp->task_next == task_list)
		{
			break;
		}
	}
}

