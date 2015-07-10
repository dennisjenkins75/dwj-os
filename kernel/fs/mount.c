/*	kernel/fs/mount.c

	Implements mount() and unmount().
*/

#include "kernel/kernel/kernel.h"

//#define T() printf("%s: %d\n", __FUNCTION__, __LINE__)
#define T()

int	vfs_mount(const char *type, const char *mntpoint, const char *src, const char *opts)
{
	struct fs_type	*fs_type = NULL;
	struct vnode	*src_vn = NULL;
	struct vnode	*root_vn = NULL;
	struct vnode	*mnt_vn = NULL;
	struct fs_mount	*mount = NULL;
	int		result = 0;

	if (!type || !mntpoint)
	{
		return -EINVAL;
	}

	if (!fs_root && strcmp(mntpoint, "/"))
	{
		return -EINVAL;
	}

	if (NULL == (fs_type = vfs_find_fs(type)))
	{
		return -ENODEV;
	}

	if ((fs_type->vfs_ops->flags & VFS_NEED_DEV) && !src)
	{
		result = -EINVAL;
		goto error;
	}

T();	if (src && src[0])
	{
		if (0 > (result = vfs_vnode_find(src, &src_vn)))
		{
			goto error;
		}
	}

T();	if (0 > (result = vfs_vnode_find(mntpoint, &mnt_vn)))
	{
		goto error;
	}

T();	if (NULL == (mount = (struct fs_mount*)kmalloc(sizeof(*mount), HEAP_FAILOK)))
	{
		result = -ENOMEM;
		goto error;
	}

T();	if (!mnt_vn)
	{
T();		if (NULL == (mnt_vn = vfs_vnode_alloc("/", NULL, mount)))
		{
			result = -ENOMEM;
			goto error;
		}

		mnt_vn->mode = S_IFDIR;
		mnt_vn->inode_num = 1;

T();		root_vn = mnt_vn;
	}
	else if (NULL == (root_vn = vfs_vnode_alloc(mnt_vn->name, mnt_vn->parent, mount)))
	{
		result = -ENOMEM;
		goto error;
	}

T();	mount->fs_type = fs_type;
	mount->src_vn = src_vn;
	mount->root = root_vn;
	mount->old = mnt_vn;
	mount->next = mount;
	mount->prev = mount;
	mount->data = NULL;

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

//	printf("vfs_mount(%s, %s, %s) done.\n", type, mntpoint, src);

	return 0;

error:
T();	if (mount)
	{
		kfree(mount);
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
