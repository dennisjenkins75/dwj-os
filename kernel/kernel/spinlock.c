/*	kernel/spinlock.c	*/

#include "kernel/kernel.h"

// Semaphore that tracks how many times interrupts have been "disabled".
// Note that the semaphore is set to "1", this is because our kernel starts
// out with interrupts DISABLED.  So the first call to enable() will decrement
// the semaphore.
int volatile interrupt_disable_sem = 1;

#if (DEBUG_INT_DISABLE)
const char *ints_on_str = "::Interrupts Enabled::";
const char *ints_off_str = "::Interrupts Disabled::";
#endif

// Called from inline ASM code.
void	panic_disable_overflow(void)
{
	PANIC1("disable() overflow\n");
}

// Called from inline ASM code.
void	panic_enable_overflow(void)
{
	PANIC1("enable() overflow\n");
}

void	test_spinlocks(void)
{
	spinlock	b = INIT_SPINLOCK("test");

	spinlock_acquire(&b);
	spinlock_release(&b);

/*
	printf("spinlock = %p (%d bytes)\n", &b, sizeof(b));
	mem_dump(&b, sizeof(b));
	Halt();
*/
}
