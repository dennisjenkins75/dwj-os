/*	kernel/lib.c

	Generic internal functions, mostly kernel-mode implementations
	of standard libc stuff, like printf, strcmp, itoa, etc...

	Several of these functions were provided by Zach Ogden of #osdev.
	He obtained them from Chris Giese.
*/

#include "kernel/kernel.h"

/*****************************************************************************
TAKEN FROM VM86_MONITOR BY CHRIS GIESE

Revised Jan 28, 2002
- changes to make characters 0x80-0xFF display properly

Revised June 10, 2001
- changes to make vsprintf() terminate string with '\0'

Revised May 12, 2000
- math in DO_NUM is now unsigned, as it should be
- %0 flag (pad left with zeroes) now works
- actually did some TESTING, maybe fixed some other bugs

	name:	do_printf
	action:	minimal subfunction for ?printf, calls function
		'fn' with arg 'ptr' for each character to be output
	returns:total number of characters output

	%[flag][width][.prec][mod][conv]
	flag:	-	left justify, pad right w/ blanks	DONE
		0	pad left w/ 0 for numerics		DONE
		+	always print sign, + or -		no
		' '	(blank)					no
		#	(???)					no

	width:		(field width)				DONE

	prec:		(precision)				no

	conv:	d,i	decimal int				DONE
		u	decimal unsigned			DONE
		o	octal					DONE
		x,X	hex					DONE
		f,e,g,E,G float					no
		c	char					DONE
		s	string					DONE
		p	ptr					DONE

	mod:	N	near ptr				DONE
		F	far ptr					no
		h	short (16-bit) int			DONE
		l	long (32-bit) int			DONE
		L	long long (64-bit) int			no
*****************************************************************************/
/* flags used in processing format string */
#define		PR_LJ	0x01	/* left justify */
#define		PR_CA	0x02	/* use A-F instead of a-f for hex */
#define		PR_SG	0x04	/* signed numeric conversion (%d vs. %u) */
#define		PR_32	0x08	/* long (32-bit) numeric conversion */
#define		PR_16	0x10	/* short (16-bit) numeric conversion */
#define		PR_WS	0x20	/* PR_SG set and num was < 0 */
#define		PR_LZ	0x40	/* pad left with '0' instead of ' ' */
#define		PR_FP	0x80	/* pointers are far */

/* largest number handled is 2^32-1, lowest radix handled is 8.
2^32-1 in base 8 has 11 digits (add 5 for trailing NUL and for slop) */
#define		PR_BUFLEN	16

static int 	do_printf(const char *fmt, va_list args, fnptr_t fn, void *ptr)
//static int 	do_printf(fnptr_t fn, void *ptr const char *fmt, va_list args)
{
	unsigned state, flags, radix, actual_wd, count, given_wd;
	unsigned char *where, buf[PR_BUFLEN];
	long num;

	state = flags = count = given_wd = 0;
/* begin scanning format specifier list */
	for(; *fmt; fmt++)
	{
		switch(state)
		{
/* STATE 0: AWAITING % */
		case 0:
			if(*fmt != '%')	/* not %... */
			{
				fn(*fmt, &ptr);	/* ...just echo it */
				count++;
				break;
			}
/* found %, get next char and advance state to check if next char is a flag */
			state++;
			fmt++;
			/* FALL THROUGH */
/* STATE 1: AWAITING FLAGS (%-0) */
		case 1:
			if(*fmt == '%')	/* %% */
			{
				fn(*fmt, &ptr);
				count++;
				state = flags = given_wd = 0;
				break;
			}
			if(*fmt == '-')
			{
				if(flags & PR_LJ)/* %-- is illegal */
					state = flags = given_wd = 0;
				else
					flags |= PR_LJ;
				break;
			}
/* not a flag char: advance state to check if it's field width */
			state++;
/* check now for '%0...' */
			if(*fmt == '0')
			{
				flags |= PR_LZ;
				fmt++;
			}
			/* FALL THROUGH */
/* STATE 2: AWAITING (NUMERIC) FIELD WIDTH */
		case 2:
			if(*fmt >= '0' && *fmt <= '9')
			{
				given_wd = 10 * given_wd +
					(*fmt - '0');
				break;
			}
/* not field width: advance state to check if it's a modifier */
			state++;
			/* FALL THROUGH */
/* STATE 3: AWAITING MODIFIER CHARS (FNlh) */
		case 3:
			if(*fmt == 'F')
			{
				flags |= PR_FP;
				break;
			}
			if(*fmt == 'N')
				break;
			if(*fmt == 'l')
			{
				flags |= PR_32;
				break;
			}
			if(*fmt == 'h')
			{
				flags |= PR_16;
				break;
			}
/* not modifier: advance state to check if it's a conversion char */
			state++;
			/* FALL THROUGH */
/* STATE 4: AWAITING CONVERSION CHARS (Xxpndiuocs) */
		case 4:
			where = buf + PR_BUFLEN - 1;
			*where = '\0';
			switch(*fmt)
			{
			case 'X':
				flags |= PR_CA;
				/* FALL THROUGH */
/* xxx - far pointers (%Fp, %Fn) not yet supported */
			case 'x':
			case 'n':
				radix = 16;
				goto DO_NUM;
			case 'p':
				given_wd = 8;
				radix = 16;
				flags |= PR_LZ;
				goto DO_NUM;
			case 'd':
			case 'i':
				flags |= PR_SG;
				/* FALL THROUGH */
			case 'u':
				radix = 10;
				goto DO_NUM;
			case 'o':
				radix = 8;
/* load the value to be printed. l=long=32 bits: */
DO_NUM:				if(flags & PR_32)
					num = va_arg(args, unsigned long);
/* h=short=16 bits (signed or unsigned) */
				else if(flags & PR_16)
				{
					if(flags & PR_SG)
						num = va_arg(args, short);
					else
						num = va_arg(args, unsigned short);
				}
/* no h nor l: sizeof(int) bits (signed or unsigned) */
				else
				{
					if(flags & PR_SG)
						num = va_arg(args, int);
					else
						num = va_arg(args, unsigned int);
				}
/* take care of sign */
				if(flags & PR_SG)
				{
					if(num < 0)
					{
						flags |= PR_WS;
						num = -num;
					}
				}
/* convert binary to octal/decimal/hex ASCII
OK, I found my mistake. The math here is _always_ unsigned */
				do
				{
					unsigned long temp;

					temp = (unsigned long)num % radix;
					where--;
					if(temp < 10)
						*where = temp + '0';
					else if(flags & PR_CA)
						*where = temp - 10 + 'A';
					else
						*where = temp - 10 + 'a';
					num = (unsigned long)num / radix;
				}
				while(num != 0);
				goto EMIT;
			case 'c':
/* disallow pad-left-with-zeroes for %c */
				flags &= ~PR_LZ;
				where--;
				*where = (unsigned char)va_arg(args,
					unsigned char);
				actual_wd = 1;
				goto EMIT2;
			case 's':
/* disallow pad-left-with-zeroes for %s */
				flags &= ~PR_LZ;
				where = va_arg(args, unsigned char *);
EMIT:
				actual_wd = strlen((char*)where);
				if(flags & PR_WS)
					actual_wd++;
/* if we pad left with ZEROES, do the sign now */
				if((flags & (PR_WS | PR_LZ)) ==
					(PR_WS | PR_LZ))
				{
					fn('-', &ptr);
					count++;
				}
/* pad on left with spaces or zeroes (for right justify) */
EMIT2:				if((flags & PR_LJ) == 0)
				{
					while(given_wd > actual_wd)
					{
						fn(flags & PR_LZ ? '0' :
							' ', &ptr);
						count++;
						given_wd--;
					}
				}
/* if we pad left with SPACES, do the sign now */
				if((flags & (PR_WS | PR_LZ)) == PR_WS)
				{
					fn('-', &ptr);
					count++;
				}
/* emit string/char/converted number */
				while(*where != '\0')
				{
					fn(*where++, &ptr);
					count++;
				}
/* pad on right with spaces (for left justify) */
				if(given_wd < actual_wd)
					given_wd = 0;
				else given_wd -= actual_wd;
				for(; given_wd; given_wd--)
				{
					fn(' ', &ptr);
					count++;
				}
				break;
			default:
				break;
			}
		default:
			state = flags = given_wd = 0;
			break;
		}
	}
	return count;
}

/*****************************************************************************
SPRINTF
*****************************************************************************/
static int vsprintf_help(unsigned c, void **ptr)
{
	char *dst;

	dst = *ptr;
	*dst++ = c;
	*ptr = dst;
	return 0 ;
}
/*****************************************************************************
*****************************************************************************/
int vsprintf(char *buffer, const char *fmt, va_list args)
{
	int ret_val;

	ret_val = do_printf(fmt, args, vsprintf_help, (void *)buffer);
	buffer[ret_val] = '\0';
	return ret_val;
}
/*****************************************************************************
*****************************************************************************/
int sprintf(char *buffer, const char *fmt, ...)
{
	va_list args;
	int ret_val;

	va_start(args, fmt);
	ret_val = vsprintf(buffer, fmt, args);
	va_end(args);
	return ret_val;
}
/*****************************************************************************
PRINTF
You must write your own putcar()
*****************************************************************************/
static int vprintf_help(unsigned c, void **ptr)
{
	con_putch(c);
	return 0 ;
}
/*****************************************************************************
*****************************************************************************/
int	vprintf(const char *fmt, va_list args)
{
	return do_printf(fmt, args, vprintf_help, NULL);
}
/*****************************************************************************
*****************************************************************************/
int	printf(const char *fmt, ...)
{
	va_list args;
	int ret_val;

	va_start(args, fmt);
	ret_val = vprintf(fmt, args);
	va_end(args);
	return ret_val;
}

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
