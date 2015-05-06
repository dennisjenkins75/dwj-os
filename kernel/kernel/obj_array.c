/*	kernel/obj_array.c

	Implements the array that we store our global set of kernel
	objects in.  Includes functions for allocating and freeing object slots.
*/

#include "kernel/kernel.h"

// Used entries have normal onode* (lsb clear).
// Unused entries form linked-list of indicies << 1, but with lsb set.
struct hnode**	_handle_array = NULL;
int		_handle_array_size = 0;
int		_handle_array_used = 0;
int		_handle_array_next_free = 0;
spinlock	_handle_array_lock = INIT_SPINLOCK("handle_array");

// Valid objects are non-null pointers with LSB cleared.
static inline int IS_VALID_HNODE(struct hnode *hnode)
{
	return hnode && !((uint32)hnode & 1);
}

static inline int IS_VALID_HANDLE(handle h)
{
	return ((int)h >= 0) && ((int)h < _handle_array_size) && IS_VALID_HNODE(_handle_array[(int)h]);
}

void	obj_init(void)
{
	int	bytes;
	int	i;

	corehelp.obj_array_ptr_ptr = (uint32)&_handle_array;
	corehelp.obj_array_size_ptr = (uint32)&_handle_array_size;

	_handle_array_size = INITIAL_OBJECT_ARRAY_SIZE;
	bytes = _handle_array_size * sizeof(struct hnode*);

	_handle_array = (struct hnode**)kmalloc(bytes, 0);
	_handle_array_used = 0;
	_handle_array_next_free = 0;

// Initialize linked list of free nodes.
	for (i = 0; i < _handle_array_size - 1; i++)
	{
		_handle_array[i] = (struct hnode*)(((i + 1) << 1) | 1);
	}
	_handle_array[i] = (struct hnode*)-1;	// All bits set = end of list.

	printf("obj: %d handle slots.\n", _handle_array_size);
}

// Searches for the named onode.  Assumes array is alreayd locked.
// Returns index into array of any matching handle if found, -ENOENT otherwise.
int	_obj_search(const char *name, enum OBJ_TYPE type)
{
	struct	hnode **hn;

	ASSERT(spinlock_is_locked(&_handle_array_lock));
	ASSERT(name);

	for (hn = _handle_array; hn < _handle_array + _handle_array_size; hn++)
	{
		if (IS_VALID_HNODE(*hn))
		{
			if ((*hn)->onode->ops->type == type &&
				(*hn)->onode->name &&
				!strcmp((*hn)->onode->name, name))
			{
				return hn - _handle_array;
			}
		}
	}

	return -ENOENT;
}


// Allocates a new handle.  Returns array index on success or <0 on error.
// Does not create the hnode or onode objects.
handle	_handle_alloc(void)
{
	int	index;

	ASSERT(spinlock_is_locked(&_handle_array_lock));

	if (_handle_array_next_free < 0)
	{
printf("_handle_alloc() returning -ENOMEM\n");
		return (handle)-ENOMEM;
	}

	index = _handle_array_next_free;
	_handle_array_next_free = (int)_handle_array[index] >> 1;
	_handle_array[index] = NULL;

	return (handle)index;
}

// Releases specified slot in object ADT.
void	_handle_free(handle h)
{
	ASSERT(spinlock_is_locked(&_handle_array_lock));
	ASSERT(IS_VALID_HANDLE(h));

	_handle_array[(int)h] = (struct hnode*)((_handle_array_next_free << 1) | 1);
	_handle_array_next_free = (int)h;
}

// Given a handle, allocates a second handle pointing to same onode,
// and increments onode's ref count.  Called by "dup" and "open".
handle	_obj_dup_internal(handle h, struct task *task)
{
	handle	h_new;
	struct hnode *hnode = NULL;

	ASSERT(spinlock_is_locked(&_handle_array_lock));
	ASSERT(IS_VALID_HANDLE(h));

	if (NULL == (hnode = (struct hnode*)kmalloc(sizeof (struct hnode), HEAP_FAILOK)))
	{
		return (handle)-ENOMEM;
	}

	if (0 > (int)(h_new = _handle_alloc()))
	{
		kfree(hnode);
		return (handle)-ENOMEM;
	}

	hnode->onode = _handle_array[(int)h]->onode;
	hnode->task = task;
	hnode->fd_offset = _handle_array[(int)h]->fd_offset;
	hnode->flags = 0;	// to be filled in by caller.

	_handle_array[(int)h_new] = hnode;
	_handle_array[(int)h]->onode->ref_count++;

	return h_new;
}

// Returns the "hnode" pointer for a given handle.
// Locks onode (to prevent others from mucking with it).
struct hnode*	_obj_get(handle h)
{
	struct hnode *hnode;

	spinlock_acquire(&_handle_array_lock);

	if (((int)h < 0) || ((int)h > _handle_array_size))
	{
		PANIC2("_obj_get(%p) handle out of range.\n", h);
	}

	if (NULL == (hnode = _handle_array[(int)h]))
	{
		PANIC2("_obj_get(%p) hnode is NULL\n", h);
	}

	if (NULL == hnode->onode)
	{
		PANIC2("_obj_get(%p) hnode->onode is NULL\n", h);
	}

	spinlock_acquire(&hnode->onode->lock);

	spinlock_release(&_handle_array_lock);

	return hnode;
}

void	_obj_release(struct hnode *hnode)
{
	spinlock_acquire(&_handle_array_lock);
	spinlock_release(&hnode->onode->lock);
	spinlock_release(&_handle_array_lock);
}


// Internally called by functions that create or open handles.
// Every open of the same object should create a unique handle.
handle	_obj_open(const char *name, uint32 flags, const struct onode_ops *ops, int extra_bytes, int *disposition)
{
	int		onode_size = 0;
	int 		index = 0;
	struct hnode	*hnode = NULL;
	struct onode	*onode = NULL;
	struct task	*task = (flags & OBJ_KERNEL) ? NULL : current;
	char		*namedup = NULL;
	handle		h = NULL;

	if (disposition)
	{
		*disposition = 0;	// default, no object was not opened.
	}

	spinlock_acquire(&_handle_array_lock);

// First, search for an existing object of the same type and name.
	if (name)
	{
		if (0 <= (index = _obj_search(name, ops->type)))
		{
			if (flags & OBJ_CREATE_NEW)
			{
				spinlock_release(&_handle_array_lock);
				return (handle)-EEXIST;
			}

// Item found, dup() the handle.
			if (0 <= (int)(h = _obj_dup_internal((handle)index, task)))
			{
				_handle_array[(int)h]->flags = flags;

				if (disposition)
				{
					*disposition = OBJ_OPEN_EXISTING;	// dup was made.
				}
			}

			spinlock_release(&_handle_array_lock);

			return h;
		}
	}

	if (flags & OBJ_OPEN_EXISTING)
	{
		spinlock_release(&_handle_array_lock);
		return (handle)-ENOENT;
	}

// Does not exist, so we must allocate.
	if (0 < (h = _handle_alloc()))
	{
		spinlock_release(&_handle_array_lock);
		return (handle)-ENOMEM;
	}

	if (NULL == (hnode = (struct hnode*)kmalloc(sizeof(*hnode), HEAP_FAILOK)))
	{
		_handle_free(h);
		spinlock_release(&_handle_array_lock);
		return (handle)-ENOMEM;
	}

	onode_size = sizeof(*onode) - sizeof(onode->extra) + extra_bytes;
	if (NULL == (onode = (struct onode*)kmalloc(onode_size, HEAP_FAILOK)))
	{
		_handle_free(h);
		kfree(hnode);
		spinlock_release(&_handle_array_lock);
		return (handle)-ENOMEM;
	}

	if (name)
	{
		if (NULL == (namedup = strdup(name)))
		{
			kfree(onode);
			_handle_free(h);
			spinlock_release(&_handle_array_lock);
			return (handle)-ENOMEM;
		}
	}

// Now that we know we won't fail out due to lack of memory, we can modify the handle array.

	onode->name = namedup;
	onode->ops = (struct onode_ops*)ops;
	spinlock_init(&onode->lock, onode->name);
	onode->ref_count = 1;
	onode->wait_list = NULL;
	onode->wait_count = 0;
	onode->signalled = 0;

	hnode->onode = onode;
	hnode->task = task;
	hnode->fd_offset = 0;
	hnode->flags = flags;

	_handle_array[(int)h] = hnode;

	spinlock_release(&_handle_array_lock);

	return h;
}

// Closes the handle.  Decrements reference count.  If count becomes 0,
// disposes of the object the handle pointed to.
int	obj_close(handle h)
{
	PANIC2("obj_close (%p) not implemented.\n", h);
	return -ENOTIMPL;
}
