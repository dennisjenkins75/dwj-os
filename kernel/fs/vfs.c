/*	kernel/fs/vfs.c

	Implements filesystem registration and deregistration.
*/

#include "kernel/kernel/kernel.h"

// Locked whenever we want to access "fs_type_list".
static struct spinlock vfs_fstable_lock = INIT_SPINLOCK("vfs_fstable");

// Points to head of linked list of registered file system types.
static struct fs_type	*fs_type_list = NULL;

// Start of all file-system lookups.
struct fs_mount	*fs_root = NULL;

// Insert new fs into linked list of file-systems.
int	vfs_register_fs(const char *name, struct vfs_ops *ops)
{
	struct fs_type *fs = NULL;
	struct fs_type *temp = NULL;

	if (!name || !ops)
	{
		return -EINVAL;
	}

	if (NULL == (fs = (struct fs_type*)kmalloc(sizeof(struct fs_type), HEAP_FAILOK)))
	{
		return -ENOMEM;
	}

	fs->name = strdup(name);
	fs->vfs_ops = ops;
	fs->ref_count = 0;
	fs->next = fs;
	fs->prev = fs;

	spinlock_acquire(&vfs_fstable_lock);

// Check to see if a filesystem by the same name is already registered.
	for (temp = fs_type_list; temp; temp = temp->next)
	{
		if (!strcmp(temp->name, name))
		{
			spinlock_release(&vfs_fstable_lock);
			kfree(fs->name);
			kfree(fs);
			return -EEXIST;
		}

		if (temp->next == fs_type_list)
		{
			break;
		}
	}

	if (fs_type_list)
	{
		fs->next = fs_type_list;
		fs->prev = fs_type_list->prev;
		fs_type_list->prev->next = fs;
		fs_type_list->prev = fs;

		ASSERT(fs->next->prev == fs);
		ASSERT(fs->prev->next == fs);
		ASSERT(fs_type_list->prev->next == fs_type_list);
		ASSERT(fs_type_list->next->prev == fs_type_list);
	}
	else
	{
		fs_type_list = fs;
	}

	spinlock_release(&vfs_fstable_lock);

	printf("fs type '%s' registered.\n", name);

	return 0;
}

int	vfs_unregister_fs(const char *name)
{
	PANIC2("vfs: unregister(%s) not implemented.\n", name);
// FIXME: Cannot unregister a filesystem if there are any open files using it.
	return -ENOTIMPL;
}

// Increments ref_count on fs_type, if object is found.
// Returns NULL if no fd_type is found.
struct fs_type*	vfs_find_fs(const char *registered_name)
{
	struct fs_type *temp = NULL;

	ASSERT(!spinlock_is_locked(&vfs_fstable_lock));

	spinlock_acquire(&vfs_fstable_lock);

	for (temp = fs_type_list; temp; temp = temp->next)
	{
		if (!strcmp(temp->name, registered_name))
		{
			temp->ref_count++;
			spinlock_release(&vfs_fstable_lock);
			return temp;
		}

		if (temp->next == fs_type_list)
		{
			break;
		}
	}

	spinlock_release(&vfs_fstable_lock);
	return NULL;
}

void	vfs_release_fs(struct fs_type *fs)
{
	ASSERT(!spinlock_is_locked(&vfs_fstable_lock));
	spinlock_acquire(&vfs_fstable_lock);

	ASSERT(fs->ref_count > 0);
	fs->ref_count--;
	spinlock_release(&vfs_fstable_lock);
}
