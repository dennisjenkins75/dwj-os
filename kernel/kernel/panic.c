/*	kernel/panic.c	*/

#include "kernel/kernel.h"

// See code in "tools/makemap.c" on how the VAST file is constructed.
const char *ResolveSymbol(uint32 addr, uint32 *offset)
{
	struct vast_hdr_t	*hdr;
	struct vast_addr_t	*vaddr;

	if (offset) *offset = 0;

	if (!g_pVastMapAddr) return "<no vast>";

	hdr = (struct vast_hdr_t*)g_pVastMapAddr;
	if (hdr->magic != VAST_MAGIC) return "<invalid vast>";

	vaddr = (struct vast_addr_t*)(hdr + 1);
	for (; vaddr->virt_addr; vaddr++)
	{
		if ((addr >= vaddr->virt_addr) && ((addr < (vaddr+1)->virt_addr) || !((vaddr+1)->virt_addr)))
		{
			if (offset) *offset = addr - vaddr->virt_addr;
			return (char*)((uint32)g_pVastMapAddr + vaddr->name_offset);
		}
	}

	return "??";
}

// Returns '1' if "ebp" is likely valid.
// Returns '0' if "ebp" probably points into deep space.
int	is_valid_frame(uint32 ebp)
{
	if ((ebp >= (uint32)&_kernel_heap_start) && (ebp < (uint32)&_kernel_heap_end))
	{
		return 1;	// NOTE: not a 100% guarantee!
	}

	if ((ebp >= (uint32)&_kernel_stack_start) && (ebp < ((uint32)&_kernel_stack_start + (uint32)&_kernel_stack_size)))
	{
		return 1;
	}

	return 0;
}

void	dump_stack(uint32 frame_addr)
{
	uint32		next_frame = 0;
	uint32		ret_addr = 0;
	const char	*symbol = NULL;
	uint32		offset = 0;

	while (1)
	{
		if (!is_valid_frame(frame_addr))
		{
			printf("\tebp:%p  --- likely invalid, stack trace terminated.\n", frame_addr);
		}

		next_frame = *(uint32*)frame_addr;
		ret_addr = *(uint32*)(frame_addr + 4);
		symbol = ResolveSymbol(ret_addr, &offset);

		if (!ret_addr)
		{
			break;
		}

		printf("\tframe:%p, ret:%p, %s + %d\n", frame_addr, ret_addr, symbol, offset);

		if (frame_addr >= next_frame)
		{
			break;
		}

		frame_addr = next_frame;
	}
}

void	panic(const char *file, const char *func, int line, const char *fmt, ...)
{
	va_list		va;
	uint32		esp;
	uint32		ebp;
	uint32		cr3;

	__asm__ __volatile__ ("cli");

	__asm__ __volatile__
	(
		"movl %%cr3, %%eax	\n"
		"movl %%eax, %0		\n"
		"movl %%ebp, %1		\n"
		"movl %%esp, %2		\n"
		: "=m"(cr3), "=m"(ebp), "=m"(esp)
		: /* inputs */
		: "%eax"
	);

	con_settextcolor(15, 4);           // white on red.
	printf("\nPanic, System Halted!    %s:%d (%s):\n", file, line, func);

	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);

	// Need these three constants to make the stack dump.
//	printf("\ncr3 = %p, ebp = %p, esp = %p\n", cr3, ebp, esp);

	printf("\n");
	dump_stack(ebp);

        for (;;)
        {
                __asm__ __volatile__ ("cli;hlt");
        }
}
