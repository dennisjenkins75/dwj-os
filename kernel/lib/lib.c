/*	kernel/lib.c

	Generic internal functions, mostly kernel-mode implementations
	of standard libc stuff, like printf, strcmp, itoa, etc...

	Several of these functions were provided by Zach Ogden of #osdev.
	He obtained them from Chris Giese.
*/

#include "kernel/kernel.h"

size_t	strlen(const char *str)
{
    size_t retval;
    for (retval = 0; *str != '\0'; str++, retval++);
    return retval;
}

char*	strdup(const char *str)
{
	size_t	len;
	char	*ret;

	for (len = 0; str[len]; len++);
	ret = (char*)kmalloc(len + 1, HEAP_FAILOK);
	for (len = 0; str[len]; ret[len] = str[len], len++);
	ret[len] = 0;

	return ret;
}

// FIXME: rewrite strcmp.  Will crash is strings are NULL.
// Also rewrite it to do only a single-pass
int	strcmp(const char *scursorPos, const char *s2)
{
	size_t size_scursorPos, size_s2; // Variables containing sizes of strings.
	int i;                   // Loop counter.
	size_scursorPos = strlen(scursorPos);    // We grab the length of the respective strings
	size_s2 = strlen(s2);    // so we can compare them. No dice, ditch.
	
	/* If they're not the same size, don't continue. */
	if(size_scursorPos != size_s2) return -1;
	
	for(i = 0; i < size_scursorPos; i++) {
		if(scursorPos[i] != s2[i]) return -1;
	}
	
	return 0;
}

// Copied from http://en.wikibooks.org/wiki/C_Programming/Strings#The_strncmp_function
int	strncmp(const char *s1, const char *s2, size_t n)
{
	unsigned char uc1, uc2;

	if (n == 0) return 0;

	while (n-- > 0 && *s1 == *s2)
	{
// If we've run out of bytes or hit a null, return zero since we already know *s1 == *s2.
		if (n == 0 || *s1 == '\0') return 0;
	        s1++;
		s2++;
	}

	uc1 = (*(unsigned char *) s1);
	uc2 = (*(unsigned char *) s2);
	return ((uc1 < uc2) ? -1 : (uc1 > uc2));
}

/* Convert the integer D to a string and save the string in BUF. If
   BASE is equal to 'd', interpret that D is decimal, and if BASE is
   equal to 'x', interpret that D is hexadecimal.  */
void	itoa(char *buf, int base, int d)
{
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;
  
  /* If %d is specified and D is minus, put `-' in the head.  */
  if (base == 'd' && d < 0)
  {
      *p++ = '-';
      buf++;
      ud = -d;
  }
  else if (base == 'x')
    divisor = 16;

  /* Divide UD by DIVISOR until UD == 0.  */
  do
  {
      int remainder = ud % divisor;
      
      *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
  }
  while (ud /= divisor);

  /* Terminate BUF.  */
  *p = 0;
  
  /* Reverse BUF.  */
  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
  {
      char tmp = *p1;
      *p1 = *p2;
      *p2 = tmp;
      p1++;
      p2--;
  }
}

void *memcpy(void *dest, const void *src, size_t count)
{
    const char *sp = (const char *)src;
    char *dp = (char *)dest;
    for(; count != 0; count--) *dp++ = *sp++;
    return dest;
}

void *memcpydw(void *dest, const void *src, size_t count)
{
    const uint32 *sp = (const uint32 *)src;
    uint32 *dp = (uint32 *)dest;
    for(; count != 0; count--) *dp++ = *sp++;
    return dest;
}

void *memset(void *dest, char val, size_t count)
{
    char *temp = (char *)dest;
    for( ; count != 0; count--) *temp++ = val;
    return dest;
}

unsigned short *memsetw(unsigned short *dest, unsigned short val, size_t count)
{
    unsigned short *temp = (unsigned short *)dest;
    for( ; count != 0; count--) *temp++ = val;
    return dest;
}

// Not more than 'n' bytes are copied.  If the first 'n' bytes of
// 'src' are not null terminated, 'dest' will not be null terminated.
// Follows "man 3 strncpy"
char	*k_strncpy(char *dest, const char *src, size_t n)
{
	char *result = dest;

	while (*src && n)
	{
		*dest = *src;
		dest++;
		src++;
		n--;
	}

	while (n)
	{
		*dest = 0;
		dest++;
		n--;
	}

	return result;
}

char	*strcpy_s(char *dest, int destlen, const char *src)
{
	char *result = dest;

	while (*src && --destlen)
	{
		*dest = *src;
		dest++;
		src++;
	}

	*dest = 0;
	return result;
}

char	*k_strstr(const char *haystack, const char *needle)
{
	char	*ptr = NULL;
	char	*p2 = NULL;
	char	*n = NULL;

	if (!needle) return (char*)haystack;

	if (!haystack) return NULL;

	for (ptr = (char*)haystack; *ptr; ptr++)
	{
		for (n = (char*)needle, p2 = ptr; *n && (*n == *p2); n++, p2++);

		if (!*n)
		{
			return ptr;
		}
	}

	return NULL;
}

int	atoi(const char *nptr)
{
	int	result = 0;

	if (!nptr) return 0;

	while (*nptr == ' ') nptr++;

	while ((*nptr >= '0') && (*nptr <= '9'))
	{
		result = result * 10 + (*nptr - '0');
		nptr++;
	}

	return result;
}

//	k_getArg()
//	Inputs: "cmdline" and "arg".
//	Will locate "arg=" on the command line and copy from after the equals 
//		sign to the next space to dest.
//	Returns: "dest" if arg found and copied, NULL if not.
//	Notes: Silently truncates argument if destlen is indicates dest is too small.
//	Notes: Does not handle quotes or escapes.

char*	k_getArg(const char *cmdline, char *dest, int destlen, const char *arg)
{
	const char	*ptr = NULL;
	int		len = arg ? strlen(arg) : 0;
	char		*dest_ptr = dest;

	if (!cmdline || !dest || !arg || !len)
	{
		return 0;
	}

	for (ptr = cmdline; *ptr; ptr++)
	{
		if (!strncmp(ptr, arg, len) && (*(ptr + len) == '='))
		{
			ptr += (len + 1);

			while (*ptr && (*ptr != ' ') && (destlen > 1))
			{
				*(dest_ptr++) = *(ptr++);
				destlen--;
			}

			*dest_ptr = 0;	// terminate string.

			return dest;
		}
	}

	return NULL;
}

unsigned char inportb (unsigned short _port)
{
    unsigned char rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

void outportb (unsigned short _port, unsigned char _data)
{
    __asm__ __volatile__ ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

uint32 inportl (uint16 _port)
{
	uint32 rv;
	__asm__ __volatile__ ("inl %1, %0" : "=a" (rv) : "dN" (_port));
	return rv;
}

void outportl(uint16 _port, uint32 _data)
{
	__asm__ __volatile__ ("outl %1, %0" : : "dN" (_port), "a" (_data));
}
