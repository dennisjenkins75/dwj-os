/* kernel/lib/printf.h */

#ifndef	__LIB_PRINTF_H__
#define	__LIB_PRINTF_H__

#define ROUND_UP(x,y) (((x) + (y) - 1) & ~((y)-1))

// Portions of this code is from Zach Ogden, who got it from Chris Giese.
// Modified by Dennis Jenkins to include buffer overflow checks.

// Defines function used to push chars onto buffer.
// Returns '1' if char was succesfully pushed (enough room to hold it).
// Returns '0' if char was not pushed (out of room).
typedef int (*fnptr_t)(unsigned char c, void **destbuffer);

extern int 	do_printf (const char *fmt, size_t maxlen, va_list args, fnptr_t fn, void *ptr);
extern int	vsnprintf(char *buffer, size_t maxlen, const char *fmt, va_list args);
extern int	snprintf (char *buffer, size_t maxlen, const char *fmt, ...);
extern int	vprintf(const char *fmt, va_list args);
extern int	printf(const char *fmt, ...);

#endif	// __LIB_PRINTF_H__
