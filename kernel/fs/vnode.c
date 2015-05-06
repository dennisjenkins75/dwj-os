/*	kernel/fs/vnode.c	*/

#include "kernel/kernel/kernel.h"

//#define T() printf("%s: %d\n", __FUNCTION__, __LINE__)
#define T()

// Creates a new vnode.  Sets ref count to 1.
// Must return NULL on failure.
struct vnode*   vfs_vnode_alloc(const char *name, struct vnode *parent, struct mount *mount)
{
	struct vnode	*vn;

	ASSERT(name);
	ASSERT(mount);
	// parent may be NULL.

	if (NULL == (vn = kmalloc(sizeof(*vn), HEAP_FAILOK)))
	{
		return NULL;
	}

	if (NULL == (vn->name = strdup(name)))
	{
		kfree(vn);
		return NULL;
	}

	vn->parent = parent;
	vn->child = NULL;
	vn->next = vn->prev = vn;
	vn->mode = 0;
	vn->inode_num = -1;
	vn->ref_count = 1;
	vn->vfs_ops = NULL;
	vn->mount = mount;
	vn->data = NULL;

	return vn;
}

// Decrements ref count.  If zero, frees vnode.
int	vfs_vnode_free(struct vnode *vnode)
{
	PANIC2("vfs_vnode_free(%p) is not implemented!", vnode);
	return -EINVAL;
}

// Returns vnode for a given fully qualified pathname.
// Returns error code if not found.
int	vfs_vnode_find(const char *name, struct vnode **vn)
{
	int		i;
	struct vnode	*tvn1;
	struct vnode	*tvn2;
	char		*tname;

	if (!name || !vn || !*name)
	{
		return -EINVAL;
	}

	*vn = NULL;

	if (!fs_root && strcmp(name, "/"))
	{
		return -EINVAL;
	}

	if (!fs_root)
	{
		*vn = NULL;
		return 0;
	}

// Start at the top.
	if (*name != '/')
	{
		return -EINVAL;
	}

	tvn1 = fs_root->root;
	tvn2 = NULL;

	do
	{
// Assume current vnode is a directory.
T();		if (!(tvn1->mode & S_IFDIR))
		{
			return -ENOENT;
		}

// Eat leading slashes.
T();		while (*name == '/') { name++; }

// Scan forward and find next seperator or NULL.
		for (i = 0; name[i] && name[i] != '/'; i++);

T();		if (NULL == (tname = kmalloc(i, HEAP_FAILOK)))
		{
			return -ENOMEM;
		}

		memcpy(tname, name, i);
		tname[i] = 0;

printf("name = '%s'\n", tname);

// Scan vnode for this file.
T();		if (NULL == tvn1->mount->fs_type->vfs_ops->find)
		{
			return -ENOTIMPL;
		}

T();		if (0 >= (i = tvn1->mount->fs_type->vfs_ops->find(tvn1, tname, &tvn2)))
		{
//			vfs_vnode_free(tvn1); // ???
			return i;
		}

		tvn1 = tvn2;	tvn2 = NULL;
		kfree(tname);	tname = NULL;

		name += i;
T();
	} while (*name);

	*vn = tvn1;

	return 0;
}
