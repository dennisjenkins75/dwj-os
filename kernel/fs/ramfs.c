/*	kernel/fs/ramfs.c

	Implements ramdisk like file-system.  Note that this is the first
	filesystem registered and mounted, and it is mounted at "/".
*/

#include "kernel/kernel/kernel.h"

//#define T() printf("%s: %d\n", __FUNCTION__, __LINE__)
#define T()

struct ramfs_inode
{
	char		*name;
	ssize_t		file_size;
	ssize_t		alloc_size;
	mode_t		mode;			// DIR, REG, etc...
	void		*data;

	struct ramfs_inode	*parent;	// NULL if "/"
	struct ramfs_inode	*child;		// NULL if not directory.
	struct ramfs_inode	*next;
	struct ramfs_inode	*prev;
};

int	ramfs_mount(struct vnode *vn, struct fs_mount *mount, const char *ops)
{
	struct ramfs_inode *root;

	ASSERT(mount);
	ASSERT(mount->root);
	ASSERT(NULL == mount->data);

	if (NULL == (root = (struct ramfs_inode*)kmalloc(sizeof(*root), HEAP_FAILOK)))
	{
		return -ENOMEM;
	}

	root->name = NULL;
	root->file_size = 0;
	root->alloc_size = 0;
	root->mode = S_IFDIR;
	root->data = NULL;

	root->parent = NULL;
	root->child = NULL;
	root->next = root;
	root->prev = root;

//printf("mount->root = %p\n", mount->root);

T();	mount->data = (void*)root;
T();	mount->root->inode_num = 1;		// FIXME: Is this correct?
T();	mount->root->data = (void*)root;

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
