/*	tools/msdev/obj_adr/obj_adt.c

	I need to design some code and debug it with a modern IDE.
	So I created this section of the source tree for small projects
	that I debug inside MS Dev Studio 98 (msvc 6).
*/

#define STRICT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#define MAX_HANDLES 2
#define MAX_TASKS	3

#define ASSERT assert
#define ASSERT_HEAP ASSERT
#define kfree(x) free(x)

typedef unsigned __int64 uint64;
typedef signed __int64 off64_t;
typedef int spinlock;
typedef int taskid_t;

struct onode;
struct hnode;
struct task;

enum	task_state
{
	RUNNABLE = 0,		// Ready to execute, but not executing.
	RUNNING = 1,		// Currently executing.
	WAITING = 2,		// Waiting for IO or some other event.
	PAUSED = 3,		// Paused by an API (not blocking on IO )
	ZOMBIE = 4		// Dead, waiting to be claimed by someone.
};

struct	wait_node
{
	struct wait_node	*task_next;
	struct wait_node	*task_prev;
	struct wait_node	*obj_next;
	struct wait_node	*obj_prev;
	struct onode		*onode;
	struct task			*task;
};

struct onode
{
	char 				*name;
//	enum OBJ_TYPE		type;
//	spinlock			lock;
	int					ref_count;
	struct wait_node	*wait_list;
	int					wait_count;
//	char				extra[4];
};

struct hnode
{
	struct onode	*onode;
//	struct task		*task;
//	off64_t			fd_offset;
//	int				flags;
};

struct	task
{
//	struct task		*task_next;
//	struct task		*task_prev;

	char			*name;
	struct wait_node	*wait_list;
	int				wait_count;
	int				wait_all;
//	uint64			wait_time;

//	taskid_t		taskid;
	enum task_state		state;
};


struct hnode	*_handle_array[MAX_HANDLES];
struct task		*_task_array[MAX_TASKS];

static int spinlock_is_locked(spinlock *sl)
{
	return *sl;
}

void	new_handle(int i)
{
	char name[32];
	struct hnode *hnode;

	sprintf(name, "obj-%c", 'a' + i);

	hnode = (struct hnode*)malloc(sizeof(struct hnode));
//	hnode->fd_offset = 0;
//	hnode->flags = 0;
//	hnode->task = NULL;
	hnode->onode = (struct onode*)malloc(sizeof(struct onode));
	hnode->onode->name = strdup(name);
//	hnode->onode->type = (enum OBJ_TYPE)0;
//	hnode->onode->lock = 0;
	hnode->onode->ref_count = 1;
	hnode->onode->wait_list = NULL;
	hnode->onode->wait_count = 0;
	
	_handle_array[i] = hnode;	
}

void	new_task(int i)
{
	char	name[32];
	struct task *task;

	sprintf(name, "t-%d", i);

	task = (struct task*)malloc(sizeof(struct task));
	task->wait_list = NULL;
	task->wait_count = 0;
	task->wait_all = 0;
//	task->wait_time = 0;
//	task->taskid = i;
	task->name = strdup(name);
	task->state = RUNNABLE;

	_task_array[i] = task;
}

// Internal function to add a wait_node between the task and a handle.
void	_obj_add_wait(struct task *task, struct hnode *hnode, struct wait_node *wn)
{
	ASSERT(task);
	ASSERT(hnode);
	ASSERT(wn);

//	ASSERT(spinlock_is_locked(&task->lock));
//	ASSERT(spinlock_is_locked(&hnode->onode->lock));

	wn->task = task;
	wn->onode = hnode->onode;
//	wn->began = time_get_abs();

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

// Do actual list insertion.
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


void	_obj_dump_wait_node(const char *header, const struct wait_node *wn)
{
//	uint8	attr = con_set_attr(0x1f);
	printf("%s: wn(%p) t=%p(id:%d, wc:%d, wl:%p)\n", header, wn, wn->task, 0/*wn->task->taskid*/, wn->task->wait_count, wn->task->wait_list);
	printf("    obj=%p (wc:%d, wl:%p)\n", wn->onode, wn->onode->wait_count, wn->onode->wait_list);
	printf("    tn=%p tp=%p\n", wn->task_next, wn->task_prev);
	printf("    on=%p op=%p\n", wn->obj_next, wn->obj_prev);
//	con_set_attr(attr);
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

//	mem_dump(wn, sizeof(*wn));

//	ASSERT(spinlock_is_locked(&wn->onode->lock));
//	ASSERT(spinlock_is_locked(&wn->task->lock));
//	ASSERT(spinlock_is_locked(&task_list_lock));

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
// Special count of "-1" means to wake ALL tasks waiting on this object.
// Returns left-over count value.
int	_obj_wake(struct hnode *hnode, int count)
{
	struct wait_node	*wn = NULL;
//	uint64			t_entry = time_get_abs();

	ASSERT(hnode);
	ASSERT((count == -1) || (count > 0));
//	ASSERT(spinlock_is_locked(&hnode->onode->lock));

//	spinlock_acquire(&task_list_lock);

	while (count && hnode->onode->wait_list)
	{
// Extract "wn" from the list.  We need this for later use.
		wn = hnode->onode->wait_list;

// Any checks against the task require that we lock it first.
//		spinlock_acquire(&wn->task->lock);

// Remove the "wn" from the ADT.
		_obj_wn_detach(wn);

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
//			wn->task->wait_time = t_entry - wn->began;

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

//		spinlock_release(&wn->task->lock);
		kfree(wn);

		if (count != -1)
		{
			count--;
		}
	}

//	spinlock_release(&task_list_lock);

	return count;
}




void	add_wait(struct task *task, struct hnode *hnode)
{
	struct wait_node	*wn = (struct wait_node*)malloc(sizeof(*wn));

	task->wait_all = 0;
	task->state = WAITING;

	_obj_add_wait(task, hnode, wn);
}


int	main(int argc, char *argv[])
{
	int		i;

	for (i = 0; i < MAX_HANDLES; new_handle(i++));
	for (i = 0; i < MAX_TASKS; new_task(i++));

	add_wait(_task_array[0], _handle_array[0]);
	add_wait(_task_array[0], _handle_array[1]);
	add_wait(_task_array[1], _handle_array[0]);
	add_wait(_task_array[1], _handle_array[1]);
	add_wait(_task_array[2], _handle_array[1]);
	add_wait(_task_array[2], _handle_array[1]);

	// simulate a "wait_many(all) for task 0".
	_task_array[0]->wait_all = 1;

	_obj_wake(_handle_array[0], -1);
	_obj_wake(_handle_array[1], 2);


	// When we are all done, there should be no waits.
	for (i = 0; i < MAX_TASKS; i++)
	{
		ASSERT(_task_array[i]->state == RUNNABLE);
	}

	for (i = 0; i < MAX_HANDLES; i++)
	{
		ASSERT(_handle_array[i]->onode->wait_count == 0);
	}

	return 0;
}