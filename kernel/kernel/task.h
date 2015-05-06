/*	kernel/task.h	*/

enum	task_state
{
	RUNNABLE = 0,		// Ready to execute, but not executing.
	RUNNING = 1,		// Currently executing.
	WAITING = 2,		// Waiting for IO or some other event.
	PAUSED = 3,		// Paused by an API (not blocking on IO )
	ZOMBIE = 4		// Dead, waiting to be claimed by someone.
};

#define TASK_STATES 5

typedef int (*entry_t)(void *arg);

struct	task
{
	struct task		*task_next;
	struct task		*task_prev;

// See comment in "kernel/objects.h".  Linked list of objects that we are waiting on.
	struct wait_node	*wait_list;
	int			wait_count;	// Count of objects that we are waiting on.
	int			wait_all;	// Flag. non-zero means wait on ALL objects.
	uint64			wait_time;	// How long function sleep for on last wait.

	spinlock		lock;
	taskid_t		taskid;
	enum task_state		state;
	int			exit_code;
	char			name[32];
	int			ring;		// i386 cpu ring for task.

	int			ticks_left;
	int			ticks_reload;

	entry_t			entry;		// Where the creator wants to start executing.
	void			*arg;		// Creator argument to pass to "entry".

	uint32			proc_esp;	// Saved ESP at time of task switch.
	uint32			proc_cr3;	// Page directory for task.
	uint32			*kstack;	// small stack for use during interrupts.
	uint32			kstack_size;	// How large is the kernel stack?
};

struct task_stats
{
	uint32			total;
	uint32			states[TASK_STATES];
};

// Used to lock "task_list".
extern spinlock                task_list_lock;

// Points to list of all tasks in the system.
extern struct task             *task_list;

// Always points to the current task (or idle_task if none).
// WIll be NULL until scheduler is initialized.
extern struct task	*current;

// taskid of the reaper task.
extern taskid_t		reaper_taskid;

// Called by kmain.  Create task struct for kmain().  kmain() becomes the idle thread.
extern void scheduler_init(void);

// Creates arbitrary kernel-mode thread.
extern taskid_t task_create(entry_t entry_point, void *arg, const char *name, enum task_state init_state);

// Used to pause/unpause a thread.
extern int task_set_state(taskid_t taskid, enum task_state state);

// Called by interrupt handler to (potentially) switch tasks.
extern void schedule(void);

// For internal use only.
extern struct task* task_get_ptr(taskid_t taskid);

// Returns current task stats.
extern int task_get_stats(struct task_stats *stats);

// Gives up remained to sceduler quantum.
extern void yield(void);

// Internal debugging function.
extern void task_dump_list(void);
