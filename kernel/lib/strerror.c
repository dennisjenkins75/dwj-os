/*	kernel/lib/strerror.c	*/

#include "kernel/kernel/kernel.h"

#define ARRAY_MAX 39

const char *_strerr[ARRAY_MAX] =
{
	"ESUCCESS",	// 0
	"EPERM",	// 1
	"ENOENT",	// 2
	"3", "4",
	"EIO",		// 5
	"6", "7", "8", "9", "10", "11",
	"ENOMEM",	// 12
	"EACCESS",	// 13
	"EFAULT",	// 14
	"15",
	"EBUSY",	// 16
	"EEXIST",	// 17
	"18",
	"ENODEV",	// 19
	"ENOTDIR",	// 20
	"EISDIR",	// 21
	"EINVAL",	// 22
	"ENFILE",	// 23
	"EMFILE",	// 24
	"25", "26", "27",
	"ENOSPC",	// 28
	"ESPIPE",	// 29
	"EROFS",	// 30
	"31",
	"EPIPE",	// 32
	"33", "34", "35", "36", "37",
	"ENOSYS"	// 38
};

const char* strerror(int error)
{
	if (error < 0) error = -error;

	if ((error >= 0) && (error < ARRAY_MAX))
	{
		return _strerr[error];
	}

	switch (error)
	{
		case EWAIT_ABANDONED:
			return "EWAIT_ABANDONED";
	}

	return "??";
}
