/*	kernel/vfs.h	*/

// Forward declarations.
struct statvfs;
struct stat;
struct dirent;
struct dentry;
struct vnode;
struct vfs_ops;
struct fs_mount;
struct fs_type;

typedef unsigned long long inode_t;

typedef unsigned int mode_t;
#define S_IFDIR		0x1000
#define S_IFDEV		0x2000
#define S_IFLNK		0x4000

// Bit-flags for "vfs_ops->flags"
#define VFS_NEED_DEV	1

// Max length of a file-system object name.
#define MAX_FNAME_LEN	255

struct stat
{
	off64_t		st_size;
	inode_t		inode;
};

struct dirent
{
	char		*fname;
	inode_t		inode;
};

struct vnode_ops
{
	int	flags;
	int	(*open)(struct vnode *vn, const char *fname, int flags, int mode);
	int	(*close)(struct vnode *vn);
	ssize_t	(*read)(struct vnode *vn, void *buffer, size_t count, off64_t offset);
	ssize_t	(*write)(struct vnode *vn, const void *buffer, size_t count, off64_t offset);
	int	(*ioctl)(struct vnode *vn, int request, ...);
	off64_t	(*lseek64)(struct vnode *vn, off64_t offset, int whence);
	int	(*fstat)(struct vnode *vn, struct stat *statbuf);
	int	(*readdir)(struct vnode *vn, struct dirent *dir, unsigned int count);
	int	(*mount)(struct vnode *vn, struct fs_mount *mount, const char *ops);
	int	(*mkdir)(struct vnode *vn, const char *fname, int flags, int mode);
	int	(*find)(struct vnode *vn, const char *name, struct vnode **result);
};

// "dentry" represents a directory (eg, named) hierarchy of an object in a file-system.

typedef struct dentry {
	struct dentry		*parent;
	struct dentry		*child;
	struct dentry		*next;
	struct dentry		*prev;

	struct vnode		*vnode;
	char			*name;		// dynamically allocated on heap.
	int			ref_count;
	struct spinlock		d_lock;
} dentry;

typedef struct vnode
{
// If the vnode is the absolute root of the global file-system, then "parent" points to the vnode itself.
	struct vnode		*parent;

// All vnodes are kept in a linked-list, for run-time debugging.
// 'next' and 'prev' to not imply any directory hierarchy relationship between vnodes.
	struct vnode		*next;
	struct vnode		*prev;

	mode_t			mode;
	ssize_t			file_size;
	ssize_t			blocks;

	inode_t			inode_num;
	int			ref_count;
	const struct vnode_ops	*vnode_ops;	// some file systems (dev) allow each file to have its own ops.
	struct fs_mount		*mount;
	void			*private_data;		// per filesystem private data.

	struct spinlock		v_lock;
} vnode;

typedef struct fs_mount
{
	const struct fs_type	*fs_type;
	struct dentry		*d_root;
	struct vnode		*v_root;	// vnode for root of this mount.
	struct vnode		*v_old;		// old copy of vnode for mount point.
	struct vnode		*v_blkdev;	// block device for mount source (if any).
	void			*data;

	struct fs_mount		*next;
	struct fs_mount		*prev;
	struct spinlock		fs_lock;
} fs_mount;

typedef struct fs_type
{
	struct fs_type		*next;
	struct fs_type		*prev;
	char			*name;
	const struct vnode_ops	*vnode_ops;
	int			ref_count;
} fs_type;

// vfs.c
// Who has "/" mounted.  Start of all filename lookups.
extern struct fs_mount	*fs_root;

/////////////////////////////////////////////////////////////////////////
// vfs.c
int	vfs_register_fs(const char *name, const struct vnode_ops *vnode_ops);
int	vfs_unregister_fs(const char *name);
struct fs_type* vfs_find_fs(const char *registered_name);
void	vfs_release_fs(struct fs_type *fs);

/////////////////////////////////////////////////////////////////////////
// vnode.c
// Creates a new vnode.  Sets ref count to 1.
int	vfs_vnode_alloc(struct vnode *parent, struct fs_mount *mount, struct vnode **vnode_new);

// Decrements ref-count of vnode.  Frees memory allocated to it if no longer in use.
int	vfs_vnode_free(struct vnode *vn);

// Resolve the name into a vnode.  Increments ref_count of vnode.
// returns '0' in success or error code on error.
int	vfs_vnode_find(const char *name, struct vnode **vn);

// Given a pathname and starting vnode, descend one directory level.
// Return resulting vnode and pointer to what is left of the path component.
int	vfs_vnode_descend (const char *pathname, struct vnode *vn_in, const char **nextpath, struct vnode **vn_out);

// Displays contents of vnode, as a debugging aid.
void	vfs_vnode_debug (struct vnode *vn, const char *extra);

/////////////////////////////////////////////////////////////////////////
// mount.c
int	vfs_mount(const char *type, const char *mntpoint, const char *src, const char *opts);

/////////////////////////////////////////////////////////////////////////
// vfs_ops.c
int	vfs_mkdir(const char *path);

// implemented in "ramfs.c"
void	ramfs_init(void);


// kernel/fs/dentry.c
struct dentry*	dentry_alloc (struct dentry *parent, const char *name);
void	dentry_free (struct dentry *d);
