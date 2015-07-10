/*	kernel/fs/mount.c

	Implements mount() and unmount().
*/

#include "kernel/kernel/kernel.h"

int	vfs_mount(const char *type, const char *mntpoint, const char *src, const char *opts)
{
	struct fs_type	*fs_type = NULL;
	struct vnode	*src_vn = NULL;
	struct vnode	*root_vn = NULL;
	struct vnode	*mnt_vn = NULL;
	struct fs_mount	*mount = NULL;
	struct dentry	*dentry = NULL;
	int		result = 0;

T();	if (!type || !mntpoint)
	{
		return -EINVAL;
	}

T();	if (!fs_root && strcmp(mntpoint, "/"))
	{
		return -EINVAL;
	}

T();	if (NULL == (fs_type = vfs_find_fs(type)))
	{
		return -ENODEV;
	}

T();	if ((fs_type->vfs_ops->flags & VFS_NEED_DEV) && !src)
	{
		result = -EINVAL;
		goto error;
	}

T();	if (src && src[0])
	{
T();		if (0 > (result = vfs_vnode_find(src, &src_vn)))
		{
T();			goto error;
		}

		vfs_vnode_debug (src_vn, "vfs_mount@src_vn");
	}

T();	if (0 > (result = vfs_vnode_find(mntpoint, &mnt_vn)))
	{
		goto error;
	}

	vfs_vnode_debug (mnt_vn, "vfs_mount@mnt_vn");
	ASSERT (0 == ((NULL == mnt_vn) ^ (NULL == fs_root)));

// If no file systems are mounted yet, create an empty root directory.
	if (!fs_root) {
		if (NULL == (dentry = dentry_alloc (NULL, ""))) {
			result = -ENOMEM;
			goto error;
		}
	} else {
		PANIC1 ("vfs_mount() not implemented for non-NULL fs_root\n");
	}

T();	if (NULL == (mount = (struct fs_mount*)kmalloc(sizeof(*mount), HEAP_FAILOK)))
	{
		result = -ENOMEM;
		goto error;
	}

T();	mount->fs_type = fs_type;
	mount->d_root = dentry;		// root directory of this mount point.
	mount->root = NULL;		// Filled in shortly.
	mount->old = NULL;		// Filled in shortly.
	mount->src_vn = src_vn;
	mount->data = NULL;
	mount->next = mount;
	mount->prev = mount;
	spinlock_init (&(mount->fs_lock), "fs_mount");

T();	if (!mnt_vn)
	{
T();		if (0 > (result = vfs_vnode_alloc (NULL, mount, &mnt_vn))) {
			goto error;
		}

		mnt_vn->mode = S_IFDIR;
		mnt_vn->inode_num = 1;

T();		root_vn = mnt_vn;
	}
	else
	{
		if (0 > (result = vfs_vnode_alloc (mnt_vn->parent, mount, &root_vn))) {
			goto error;
		}
	}

T();	mount->root = root_vn;
	mount->old = mnt_vn;

T();	if (fs_type->vfs_ops->mount)
	{
		if (0 > (result = fs_type->vfs_ops->mount(src_vn, mount, opts)))
		{
			goto error;
		}
	}

T();	if (fs_root)
	{
		mount->next = fs_root;
		mount->prev = fs_root->prev;
		fs_root->prev->next = mount;
		fs_root->prev = mount;

		ASSERT(mount->next->prev == mount);
		ASSERT(mount->prev->next == mount);
		ASSERT(fs_root->next->prev == fs_root);
		ASSERT(fs_root->prev->next == fs_root);
	}
	else
	{
		fs_root = mount;
	}

	printf("vfs_mount(%s, %s, %s) done.\n", type, mntpoint, src);

	return 0;

error:
T();	if (mount)
	{
		kfree(mount);
	}

	if (dentry) {
		dentry_free (dentry);
	}

	if (root_vn)
	{
		vfs_vnode_free(root_vn);
	}

	if (mnt_vn)
	{
		vfs_vnode_free(mnt_vn);
	}

	if (src_vn)
	{
		vfs_vnode_free(src_vn);
	}

	if (fs_type)
	{
		vfs_release_fs(fs_type);
	}

	return result;
}
