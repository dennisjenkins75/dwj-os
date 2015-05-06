/*	kernel/breakpoint.c

	http://en.wikipedia.org/wiki/Debug_register
*/

#include "kernel/kernel.h"

void	set_breakpoint(void *addr)
{
	__asm__ __volatile__
	(
		"movl	%0, %%eax\n\t"
		"movl	%%eax, %%dr0\n\t"
		"movl	%%dr7, %%eax\n\t"
		"orl	$0x03030002, %%eax\n\t"
		"movl	%%eax, %%dr7\n\t"
		: /* output */
		: "m"(addr) /* input */
		: "%eax" /* clobbers */
	);
}

void	clear_breakpoint(void *addr)
{
	__asm__ __volatile__
	(
		"xorl	%%eax, %%eax\n\t"
		"movl	%%eax, %%dr0\n\t"
		"movl	%%dr7, %%eax\n\t"
		"andl	$0xfcfcfffd, %%eax\n\t"
		"movl	%%eax, %%dr7\n\t"
		: /* output */
		: /* input */
		: "%eax" /* clobbers */
	);
}
