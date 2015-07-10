//	kernel/fs/dentry.c

#include "kernel/kernel/kernel.h"

// Allocates a new dentry node.
struct dentry*	dentry_alloc (struct dentry *parent, const char *name)
{
	struct dentry *d = NULL;
	char *name_tmp = NULL;

	ASSERT (name);

	if (NULL == (name_tmp = strdup (name))) {
		goto error;
	}

	if (NULL == (d = (struct dentry*)kmalloc (sizeof (struct dentry), HEAP_FAILOK))) {
		goto error;
	}

// FIXME: Need to lock parent, merge into tree.
	if (parent) {
		PANIC3 ("dentry_alloc(%p, %s): Code unfinished!\n", parent, name);
	}

	if (parent) {
		parent->ref_count++;	// FIXME: not thread safe.
	}

	d->parent = parent;
	d->child = NULL;
	d->next = NULL;
	d->prev = NULL;
	d->vnode = NULL;
	d->name = name_tmp;
	d->ref_count = 1;
	spinlock_init (&(d->d_lock), "dentry");

	return d;

error:
	if (d) {
		kfree (d);
	}

	if (name_tmp) {
		kfree (name_tmp);
	}

	return NULL;
}

void	dentry_free (struct dentry *d)
{
	PANIC2 ("dentry_free(%p) not implemented!\n", d);
// need to free "d->name"
// deal w/ ref_count.
// unlink from tree.

	kfree (d);
}
