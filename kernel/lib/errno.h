/*	kernel/errno.h	*/

#define EPERM		1	/* operation not permitted */
#define ENOENT		2	/* no such entity (file, directory, object) */
#define EIO		5	/* I/O error */
#define ENOMEM		12	/* out of memory */
#define EACCESS		13	/* permission denied, no access. */
#define EFAULT		14	/* invalid address */
#define EBUSY		16	/* request can't complete now, resource in use */
#define EEXIST		17	/* object already exists */
#define ENODEV		19	/* no such device or registered filesystem type */
#define ENOTDIR		20	/* not a directory when one is expected */
#define EISDIR		21	/* object is a directory when one was not expected */
#define EINVAL		22	/* invalid argument */
#define ENFILE		23	/* too many files, objects */
#define EMFILE		24	/* too many open files */
#define ENOSPC		28	/* no space left in filesystem */
#define ESPIPE		29	/* attempt to seek on non-seekable object */
#define EROFS		30	/* read only file system */
#define EPIPE		32	/* read / write to a broken pipe */

#define ENOSYS		38	/* system call not implemented */
#define ENOTIMPL	ENOSYS	/* feature not implemented */
#define EWAIT_ABANDONED	99	/* Owner of a wait object killed while holding object.
				   This value is returned from obj_wait() or obj_wait_many(). */
