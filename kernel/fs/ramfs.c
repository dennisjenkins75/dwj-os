/*	kernel/fs/ramfs.c

	Implements ramdisk like file-system.  Note that this is the first
	filesystem registered and mounted, and it is mounted at "/".
*/

#include "kernel/kernel/kernel.h"

static struct vfs_ops ramfs_ops;

int	ramfs_mount(struct vnode *vn, struct fs_mount *mount, const char *ops)
{
	ASSERT(mount);
	ASSERT(mount->root);
	ASSERT(NULL == mount->data);

T();	return 0;
}

/*
int	ramfs_namei(void *p, const char *path, int *inode)
{
	return -ENOTIMPL;
}
*/

static struct vfs_ops ramfs_ops =
{
	.flags = 0,
	.mount = ramfs_mount,
//	.namei = ramfs_namei
};

void	ramfs_init(void)
{
	vfs_register_fs("ramfs", &ramfs_ops);
}
