/* kernel/lib.h */

#ifndef	__LIB_H__
#define	__LIB_H__

#define ROUND_UP(x,y) (((x) + (y) - 1) & ~((y)-1))

/* lib.c */
extern size_t	strlen(const char *str);
extern int	strcmp(const char *scursorPos, const char *s2);
extern int	strncmp(const char *s1, const char *s2, size_t n);
extern char*	strdup(const char *str);
extern void	itoa(char *buf, int base, int d);
extern void	*memcpy(void *dest, const void *src, size_t count);
extern void	*memcpydw(void *dest, const void *src, size_t count);
extern void	*memset(void *dest, char val, size_t count);
extern int	memcmp (const void *a, const void *b, size_t count);
extern unsigned short *memsetw(unsigned short *dest, unsigned short val, size_t count);
extern char	*k_strncpy(char *dest, const char *src, size_t n);
extern char	*strcpy_s(char *dest, int destlen, const char *src);
extern char	*k_strstr(const char *haystack, const char *needle);
extern int	atoi(const char *nptr);
extern char*	k_getArg(const char *cmdline, char *dest, int destlen, const char *arg);

extern unsigned char inportb (unsigned short _port);
extern void	outportb (unsigned short _port, unsigned char _data);

extern uint32	inportl(uint16 _port);
extern void	outportl(uint16 _port, uint32 _data);

// strerror.c
extern const char *strerror(int error);

#endif	// __LIB_H__
