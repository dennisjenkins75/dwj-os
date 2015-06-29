/* kernel/lib/printf.h */

#ifndef	__LIB_PRINTF_H__
#define	__LIB_PRINTF_H__

#define ROUND_UP(x,y) (((x) + (y) - 1) & ~((y)-1))

// Portions of this code is from Zach Ogden, who got it from Chris Giese.
// Modified by Dennis Jenkins to include buffer overflow checks.
typedef int (*fnptr_t)(unsigned int c, void **helper);
extern int 	do_printf (const char *fmt, va_list args, fnptr_t fn, void *ptr);
extern int	vsprintf(char *buffer, const char *fmt, va_list args);
extern int	snprintf (char *buffer, size_t max, const char *fmt, ...);
extern int	sprintf(char *buffer, const char *fmt, ...);
extern int	vprintf(const char *fmt, va_list args);
extern int	printf(const char *fmt, ...);

#endif	// __LIB_PRINTF_H__
