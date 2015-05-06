/*	kernel/pagefault.c

	Implement page-fault handler (int 14).
*/

#include "kernel/kernel.h"

// Returns '1' if it handles the page fault, 0 if not.
// Generally, returning '0' will cause a kernel panic.
int	vmm_page_fault(struct regs *r, void *cr2_value)
{
// Was this in the kernel's heap?  If so, we'll map more pages into the heap.
// Otherwise, panic.

	if ((cr2_value >= (void*)&_kernel_heap_start) && (cr2_value <= (void*)&_kernel_heap_end))
	{
		heap_grow(r, cr2_value);
		return 1;
	}

	printf("Page fault for %p.  Heap from %p to %p\n",
		cr2_value, (void*)&_kernel_heap_start, (void*)&_kernel_heap_end);

	return 0;
}
