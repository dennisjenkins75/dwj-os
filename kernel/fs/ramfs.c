/*	kernel/fs/ramfs.c

	Implements ramdisk like file-system.  Note that this is the first
	filesystem registered and mounted, and it is mounted at "/".
*/

#include "kernel/kernel/kernel.h"

static struct vnode_ops ramfs_vnode_ops;

int	ramfs_fs_mount (
	struct fs_type *fs_type,
	struct blkdev *blkdev,
	struct vnode *vn,
	const char *opts,
	struct fs_mount **fs_mount
) {
	PANIC2 ("ramfs_fs_mount(%s) not implemented!\n", fs_type->name);
	return 0;
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
	.mkdir = ramfs_mkdir,
	.find = ramfs_find,
};

static struct fs_type_ops ramfs_fs_type_ops = {
	.mount = ramfs_fs_mount,
};

void	ramfs_init(void)
{
	vfs_register_fs("ramfs", &ramfs_vnode_ops, &ramfs_fs_type_ops);
}
