/*	kernel/fs/ramfs.c

	Implements ramdisk like file-system.  Note that this is the first
	filesystem registered and mounted, and it is mounted at "/".
*/

#include "kernel/kernel/kernel.h"

static struct vnode_ops ramfs_vnode_ops;

int	ramfs_mount(struct vnode *vn, struct fs_mount *mount, const char *ops)
{
	ASSERT(mount);
	ASSERT(mount->v_root);
	ASSERT(NULL == mount->data);

T();	return 0;
}

int	ramfs_mkdir (struct vnode *vn, const char *fname, int flags, int mode)
{
	PANIC2 ("ramfs_mkdir(%s) not implemented!\n", fname);
	return 0;
}

/*
int	ramfs_namei(void *p, const char *path, int *inode)
{
	return -ENOTIMPL;
}
*/

int	ramfs_find (struct vnode *vn, const char *name, struct vnode **result)
{
	PANIC3 ("ramfs_find (%p, %s) not implemented!\n", vn, name);
}

static struct vnode_ops ramfs_vnode_ops =
{
	.flags = 0,
	.mount = ramfs_mount,
//	.namei = ramfs_namei
	.mkdir = ramfs_mkdir,
	.find = ramfs_find,
};

void	ramfs_init(void)
{
	vfs_register_fs("ramfs", &ramfs_vnode_ops);
}
