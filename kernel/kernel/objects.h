/*	kernel/objects.h

	Everything in the kernel is an object (files, mutexes, semaphores, tasks).
	All objects are accessed through handles (integers), can be "waited" on, have names
	(that exist in a name-space).

	This object manager manages the name-space, and handles object allocation / de-allocation
	and converting opaque handles into object instance pointers (ie, objects in the c++ sense).

	There are a few things which are NOT objects (spinlocks, interrupts).

	In kernel-space all objects have unique IDs.  In user-space, every task has its own
	set of "file descriptors" (also integers).  These file descriptors get mapped
	to object handles in the syscall wrappers.

	No matter which task is "current", the kernel can manipulate any object (even objects
	in use by a non-current task).

	The kernel will allocate and use some objects not owned by any task.

	All valid handles have positive values when cast as integers.

	All functions and variables beginning with "_obj" should only be
	used by the object manager or object implementation code.  Ie,
	the vfs or vmm should not poke around with them.

*/

// Object types
enum OBJ_TYPE
{
	OBJ_NULL = 0,		// Temporary type used when nothing else makes sense.
	OBJ_FILE,		// An opened file in the file system.
	OBJ_TASK,		// A task (schedulable thread of execution).
	OBJ_SEMAPHORE,		// A counting semaphore.
	OBJ_MUTEX,		// Optimized, special case of a semaphore.
	OBJ_TIMER,		// Synchronous or asynchronous timer.
	OBJ_EVENT,		// Manual reset status events.
	OBJ_SOCKET,		// Network (TCP, UDP) socket.
	OBJ_MSG_QUEUE,		// message queue.
};


// Flags passed to functions that create / open objects:

#define OBJ_CREATE_NEW		0x01	/* fails if object already exists */

#define OBJ_OPEN_EXISTING	0x02	/* fails if object does NOT exist */

#define OBJ_APPEND		0x04	/* for files, always seek to end before writing */

#define OBJ_READ		0x08	/* request read rights. */

#define OBJ_WRITE		0x10	/* request write rights. */

#define OBJ_SYNC		0x20	/* request rights to "wait" on handle. */

#define OBJ_TRUNC		0x40	/* truncate / reset upon opening */

#define OBJ_KERNEL		0x8000	/* only valid from kernel.  makes handle appear in all
					   tasks, but error if user-mode syscall references it. */

#define IS_LIKELY_HANDLE(x) ((int)(x) >= 0)

struct onode_ops;
struct onode;
struct hnode;
struct task;

// When a task waits on one or more objects, the task points to a linked list
// of "wait_node"s.  When an ojbect is waited on by one or more tasks,
// the object points to a linked list of "wait_nodes".  This 2-dimensional
// mesh of nodes tracks every active wait condition.  Nodes are added and removed
// in FIFO order (no priority inversion).

struct	wait_node
{
	struct wait_node	*task_next;
	struct wait_node	*task_prev;
	struct wait_node	*obj_next;
	struct wait_node	*obj_prev;
	struct onode		*onode;
	struct task		*task;
	uint64			began;	// timestamp of when wait started.
};

// Each onode type has a variety of "operations" that can be done on it.
struct onode_ops
{
	enum OBJ_TYPE	type;

	void	(*unsignal)(struct hnode *hnode, struct task *task);
	void	(*close)(struct hnode *hnode);
};


// Internal representation of an opened object.
struct onode
{
	struct onode_ops	*ops;	// per-type structure.
	char 		*name;		// NULL if no name.
	spinlock	lock;
	int		ref_count;	// # of open handles for this object.

	struct wait_node	*wait_list;	// linked list of tasks waiting on this object.
	int		wait_count;	// # of tasks waiting on this object.
	int		signalled;	// Object is currently signalled.  Only flushed when object is unlocked.

	char		extra[4];	// extra bytes are placed here.
};

// A handle is an index into a global array of pointers to these:
// Multiple handles can point to the same onode, but have different
// file pointer offsets and flags (read/write/sync permissions).
struct hnode
{
	struct onode	*onode;
	struct task	*task;		// NULL if private to kernel.
	off64_t		fd_offset;	// Each handle has it's own "file pointer"
	int		flags;
};

// Global array of allocated objects.
extern struct hnode	**_handle_array;

// Number of elements in "_handle_array".
extern int		_handle_array_size;

// Number of allocated handles.
extern int		_handle_array_used;

// Index of free-list in handle array.
// Cast "onode" as integer index to chain to next free node.
// final free node has "-1" for next link.
extern int		_handle_array_next_free;

// Guards access to handle array.  Used when allocating or deallocating
// a handle slot.  Generally not needed when just using an object.
extern spinlock		_handle_array_lock;


// Internal functions.

extern handle	_handle_alloc(void);

extern handle	_obj_open(const char *name, uint32 flags, const struct onode_ops *ops, int extra_bytes, int *disposition);

// Returns "hnode" for the handle.  Locks the hnode.  Must call "_obj_release(hnode) when done!"
extern struct hnode*	_obj_get(handle h);

// Releases the lock (from _obj_get) on the handle.  May panic if you give it dirty looks.
extern void	_obj_release(struct hnode *hnode);

extern handle	_obj_dup_internal(handle h, struct task *task);

extern int	_obj_wake(struct hnode *hnode, int count);

// Functions usable from outside the object manager.


// Initialize the object management system.  Called only once during kernel init.
void		obj_init(void);

// Decrements reference count on object and closes it if no longer in use.
int		obj_close(handle h);

// Returns object type.
enum OBJ_TYPE	obj_type(handle h);

// Duplicates an object handle.
handle		obj_dup(handle h, uint32 flags);

// Waits for the object to be "signalled".  This means different things for differnet object types.
int		obj_wait(handle h);

// Waits for one or more handles to signal.
int		obj_wait_many(int count, const handle *hlist, int wait4all);

// "signal" or "post" to objects that support that operation (mutex, semaphore)
int		obj_post(handle h);


////////////// Object specific functions.

#define SEM_MAX_VALUE MAX_INT

extern handle	sem_open(const char *name, int flags, int *disposition, int max, int initial);

extern int	sem_release(handle h, int count);
