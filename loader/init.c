/*	loader/init.c

	Main 'C' entry point into LOADER.COM.  Called from loader/start.S.

	http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
*/

asm(".code16");

#include <stdarg.h>

// functions writen in external .S (assmebly) files.
void	testfunc(void);

extern unsigned char boot_dev_id;

static void print_char(char ch)
{
	asm volatile (
		"movw $7, %%bx\n\t"
		"movb $0x0e, %%ah\n\t"
		"movb %0, %%al\n\t"
		"int $0x10\n\t"
		: /* output */
		: /* input */		"r"(ch)
		: /* clobbered */	"%ax", "%bx"
	);
}

static void print_str(const char *str)
{
	asm volatile (
		"movw $7, %%bx\n\t"
		"movb $0x0e, %%ah\n\t"
		"movl %0, %%esi\n\t"	/* src = edi */
	"print_str_loop:\n\t"
		"lodsb\n\t"
		"or %%al, %%al\n\t"
		"jz print_str_done\n\t"
		"int $0x10\n\t"
		"jmp print_str_loop\n\t"
	"print_str_done:\n\t"
		: /* output */
		: /* input */		"r"(str)
		: /* clobbered */	"%ax", "%si", "%bx"
	);
}

static const char *text = "This is C code text.\r\n";

/* <stdarg.h> triggers gcc to want this... */
int	puts(const char *s)
{
	print_str(s);
	print_str("\r\n");
	return 0;
}

int	printf(const char *fmt, ...)
{
//	va_list	va;
	int	i = 0;

//	va_start(va, fmt);

//	for (i = 0; fmt[i]; print_char(fmt[i]));
	print_str(fmt);

//	va_end(va);

	return i;
}

static const char *test_1 = "print_char() test.\n\r";
static const char *test_2 = "print_str() test.\n\r";
static const char *test_3 = "vv_test\n\r";
static const char *test_4 = "vs_test (static global).\n\r";

void	vv_test(void)
{
	print_str(test_3);
}

void	vs_test(const char *str)
{
	print_str(str);
}

void	init_loader(void)
{
	int	i;

	for (i = 0; test_1[i]; print_char(test_1[i++]));

	print_str(test_2);

	testfunc();

	vv_test();

	asm ("int $3\n");

	vs_test(test_4);

	vs_test("vs_test #2\n\r");

	printf("\r\nprintf test!\r\n");
}
