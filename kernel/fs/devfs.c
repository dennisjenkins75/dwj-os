/*	kernel/devfs.c	*/

#include "kernel/kernel.h"


int	devfs_open(struct vnode *vn, const char *fname, int flags, int mode)
{
	return -ENOTIMPL;
}

int	devfs_close(struct vnode *vn)
{
	return -ENOTIMPL;
}

ssize_t	devfs_read(struct vnode *vn, void *buffer, size_t count, off64_t offset)
{
	return -ENOTIMPL;
}

ssize_t	devfs_write(struct vnode *vn, const void *buffer, size_t count, off64_t offset)
{
	return -ENOTIMPL;
}

off64_t	devfs_lseek64(struct vnode *vn, off64_t offset, int whence)
{
	return -ENOTIMPL;
}

int	devfs_fstat(struct vnode *vn, struct stat *statbuf)
{
	return -ENOTIMPL;
}

int     devfs_readdir(struct vnode *vn, struct dirent *dir, unsigned int count)
{
	return -ENOTIMPL;
}

static struct vnode_ops devfs_vnode_ops =
{
	.open = devfs_open,
	.close = devfs_close,
	.read = devfs_read,
	.write = devfs_write,
	.lseek64 = devfs_lseek64,
	.fstat = devfs_fstat,
	.readdir = devfs_readdir
};

static struct fs_type_ops devfs_fs_type_ops = {
};

void	devfs_init(void)
{
	vfs_register_fs("devfs", &devfs_vnode_ops, &devfs_fs_type_ops);
}
