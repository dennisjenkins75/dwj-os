/*	kernel/fs/ramfs.c

	Implements ramdisk like file-system.  Note that this is the first
	filesystem registered and mounted, and it is mounted at "/".
*/

#include "kernel/kernel/kernel.h"

static struct vfs_ops ramfs_ops;

int	ramfs_mount(struct vnode *vn, struct fs_mount *mount, const char *ops)
{
	PANIC4("ramfs_mount(%p,%p,%s) is not implemented!", vn, mount, ops);
#if 0
	struct vnode *root;

	ASSERT(mount);
	ASSERT(mount->root);
	ASSERT(NULL == mount->data);

	if (NULL == (root = (struct vnode*)kmalloc(sizeof(*root), HEAP_FAILOK)))
	{
		return -ENOMEM;
	}

	root->file_size = 0;
	root->mode = S_IFDIR;

	root->parent = NULL;
	root->child = NULL;
	root->next = root;
	root->prev = root;

	root->mount = mount;
	root->inode_num = 1;			// FIXME: Not sure where to start...
	root->ref_count = 1;
	root->vfs_ops = ramfs_ops;

printf("mount->root = %p\n", mount->root);

T();	mount->data = (void*)root;
T();	mount->root->inode_num = 1;		// FIXME: Is this correct?
T();	mount->root->data = (void*)root;
#endif
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
