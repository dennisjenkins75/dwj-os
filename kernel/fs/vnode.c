/*	kernel/fs/vnode.c	*/

#include "kernel/kernel/kernel.h"

// Creates a new vnode.  Sets ref count to 1.
int	vfs_vnode_alloc(struct vnode *parent, struct fs_mount *mount, struct vnode **vnode_new)
{
	struct vnode	*vn = NULL;

T();	ASSERT (vnode_new);
T();	ASSERT (NULL == *vnode_new);
T();	ASSERT (mount);
T();	ASSERT (mount->fs_type);
T();	ASSERT (mount->fs_type->vfs_ops);
	// parent may be NULL.

T();	if (NULL == (vn = kmalloc(sizeof(struct vnode), HEAP_FAILOK))) {
		return -ENOMEM;
	}

	vn->parent = parent;
	vn->next = vn->prev = vn;
	vn->mode = 0;
	vn->file_size = 0;
	vn->blocks = 0;
	vn->inode_num = -1;
	vn->ref_count = 1;
T();	vn->vfs_ops = mount->fs_type->vfs_ops;
	vn->mount = mount;
	vn->private_data = NULL;
T();	spinlock_init (&(vn->v_lock), "vnode");

T();	*vnode_new = vn;
	return 0;
}

// Decrements ref count.  If zero, frees vnode.
int	vfs_vnode_free(struct vnode *vnode)
{
	PANIC2("vfs_vnode_free(%p) is not implemented!", vnode);
	return -EINVAL;
}


// Give a pathname (possibly containing multiple directories), and
// a starting vnode (vn_in), descend down the path one level.
// Sets vn_out to vnode for the first path component (up to NULL or '/'),
// Sets nextpath to pointer to remainder of the pathname.
// Returns <0 on error, 0 or more is count of '/' left in nextpath.

int	vfs_vnode_descend (
	const char *pathname,		// in
	struct vnode *vn_in,		// in
	const char **nextpath,		// out (points into 'pathname')
	struct vnode **vn_out		// out
) {
	if (!pathname || !vn_in || !vn_out || !nextpath) {
		return -EINVAL;
	}

	if (!(vn_in->mode & S_IFDIR)) {
		return -ENOTDIR;
	}

	*vn_out = NULL;
	*nextpath = NULL;

// Determine if 'pathname' contains a path seperator ('/')
	char	tmpname[MAX_FNAME_LEN + 1];
	char	*t = tmpname;
	int	i = 0;

	for (i = 0; (i < MAX_FNAME_LEN) && *pathname && (*pathname != '/'); i++) {
		*(t++) = *(pathname++);
	}
	*t = 0;

	if (i >= MAX_FNAME_LEN) {
		return -EBADPATH;
	}

	if (*pathname == '/') {
		*nextpath = ++pathname;
	}

// Need to search 'vn_in' (a directory) for 'tmpname' and obtain 'tmpname's vnode.
T();	if (NULL == vn_in->vfs_ops->find) {
		return -ENOTIMPL;
	}

T();	if (0 > (i = vn_in->vfs_ops->find (vn_in, tmpname, vn_out))) {
		return i;
	}

// Return count number of '/' remaining.
	i = 0;
T();	for (const char *p = *nextpath; *p; ++p) {
		i += (*p == '/');
	}

T();	return i;
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

	tvn1 = fs_root->v_root;
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

void	vfs_vnode_debug (struct vnode *vn, const char *extra)
{
	printf ("vfs_vnode_debug (%p): %s\n", vn, extra ? extra : "");
	if (!vn) {
		return;
	}

	printf ("vn->parent    = %p\n", vn->parent);
	printf ("vn->next      = %p\n", vn->next);
	printf ("vn->prev      = %p\n", vn->prev);
	printf ("vn->mode      = %04x\n", vn->mode);
	printf ("vn->file_size = %lu\n", vn->file_size);
	printf ("vn->blocks    = %lu\n", vn->blocks);
	printf ("vn->inode_num = %ld\n", vn->inode_num);
	printf ("vn->ref_count = %d\n", vn->ref_count);
	printf ("vn->mount     = %p\n", vn->mount);
	printf ("vn->privdata  = %p\n", vn->private_data);
}

