/*	kernel/vfs.h	*/

// Forward declarations.
struct statvfs;
struct stat;
struct dirent;
struct vnode;
struct vfs_ops;
struct fs_mount;
struct fs_type;


typedef unsigned int mode_t;
#define S_IFDIR		0x1000
#define S_IFDEV		0x2000
#define S_IFLNK		0x4000

// Bit-flags for "vfs_ops->flags"
#define VFS_NEED_DEV	1

struct statvfs
{
	uint64		total_blocks;
	uint64		free_blocks;
	uint32		block_size;
};

struct stat
{
	off64_t		st_size;
	int		inode;
};

struct dirent
{
	char		*fname;
	int		inode;
};

struct vfs_ops
{
	int	flags;
	int	(*statvfs)(struct vnode *vn, struct statvfs *statvfs);
//	int	(*namei)(struct vnode *vn, const char *path, int *inode_num);
//	int	(*geti)(struct fs_mount *mnt, int inode_num, struct vnode *vn);
//	int	(*puti)(struct fs_mount *mnt, int inode_num, struct vnode *vn);
	int	(*open)(struct vnode *vn, const char *fname, int flags, int mode);
	int	(*close)(struct vnode *vn);
	ssize_t	(*read)(struct vnode *vn, void *buffer, size_t count, off64_t offset);
	ssize_t	(*write)(struct vnode *vn, const void *buffer, size_t count, off64_t offset);
	int	(*ioctl)(struct vnode *vn, int request, ...);
	off64_t	(*lseek64)(struct vnode *vn, off64_t offset, int whence);
	int	(*fstat)(struct vnode *vn, struct stat *statbuf);
	int	(*readdir)(struct vnode *vn, struct dirent *dir, unsigned int count);
	int	(*mount)(struct vnode *vn, struct fs_mount *mount, const char *ops);

	int	(*find)(struct vnode *vn, const char *name, struct vnode **result);
};

typedef struct vnode
{
	struct vnode		*parent;
	struct vnode		*child;
	struct vnode		*next;
	struct vnode		*prev;

	char			*name;
	mode_t			mode;
	int			inode_num;
	int			ref_count;
	struct vfs_ops		*vfs_ops;	// some file systems (dev) allow each file to have its own ops.
	struct fs_mount		*mount;
	void			*data;		// per filesystem private data.
} vnode;

typedef struct fs_mount
{
	struct fs_type		*fs_type;
	struct vnode		*root;		// vnode for root of this mount.
	struct vnode		*old;		// old copy of vnode for mount point.
	struct vnode		*src_vn;	// block device for mount source (if any).
	void			*data;

	struct fs_mount		*next;
	struct fs_mount		*prev;
} fs_mount;

typedef struct fs_type
{
	struct fs_type		*next;
	struct fs_type		*prev;
	char			*name;
	struct vfs_ops		*vfs_ops;
	int			ref_count;
} fs_type;

// vfs.c
// Who has "/" mounted.  Start of all filename lookups.
extern struct fs_mount	*fs_root;

/////////////////////////////////////////////////////////////////////////
// vfs.c
int	vfs_register_fs(const char *name, struct vfs_ops *ops);
int	vfs_unregister_fs(const char *name);
struct fs_type* vfs_find_fs(const char *registered_name);
void	vfs_release_fs(struct fs_type *fs);

/////////////////////////////////////////////////////////////////////////
// vnode.c
// Creates a new vnode.  Sets ref count to 1.
struct vnode*	vfs_vnode_alloc(const char *name, struct vnode *parent, struct fs_mount *mount);

// Decrements ref-count of vnode.  Frees memory allocated to it if no longer in use.
int	vfs_vnode_free(struct vnode *vn);

// Resolve the name into a vnode.  Increments ref_count of vnode.
// returns '0' in success or error code on error.
int	vfs_vnode_find(const char *name, struct vnode **vn);

/////////////////////////////////////////////////////////////////////////
// mount.c
int	vfs_mount(const char *type, const char *mntpoint, const char *src, const char *opts);

/////////////////////////////////////////////////////////////////////////
// vfs_ops.c
int	vfs_mkdir(const char *path);

// implemented in "ramfs.c"
void	ramfs_init(void);
