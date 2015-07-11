/*	kernel/fs/vfs_ops.c

	Implements top-level vfs entry points, like "open" and "mkdir".
*/

#include "kernel/kernel/kernel.h"

int	vfs_mkdir(const char *path)
{
	if (!path) {
		return -EINVAL;
	}

// FIXME: Temporary, only handles absolute paths.
	if (path[0] != '/') {
		return -EINVAL;
	}

	if (!fs_root) {
		return -ENODEV;
	}

	const char *nextpath = NULL;
	struct vnode *vn = fs_root->v_root;
	int r;

	while (1) {
T();		if (0 > (r = vfs_vnode_descend (path, vn, &nextpath, &vn))) {
T();			return r;
		}

		if (!(vn->mode & S_IFDIR)) {
T();			return -ENOTDIR;
		}

		if (r > 0) {
T();			path = nextpath;
			continue;
		}
	}

printf ("vfs_mkdir: 'nextpath' = '%s'\n", nextpath);

	return -ENOTIMPL;
}
