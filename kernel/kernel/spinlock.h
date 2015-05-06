/*	kernel/spinlock.h

	Implements kernel spinlock primatives.
*/

// NOTE: This structure's members must all be valid and represent "no lock"
// if cleared with bzero() on initialization.
typedef struct	spinlock
{
	int	lock;			// Holds actual lock flag.
	char	*name;			// optional, for debugging.
} spinlock;

// Usage: spinlock xyz_lock = INIT_SPINLOCK("xyz");
#define INIT_SPINLOCK(n) {0, n}

// See comment in "spinlock.c"
extern int volatile interrupt_disable_sem;

extern void	panic_disable_overflow(void);
extern void	panic_enable_overflow(void);

// FIXME: This method of calling "PANIC" will leave the stack fubared.
static inline void	disable(void)
{
	__asm__ __volatile__
	(
		"cli				\n"
		"lock				\n"
		"incl	_interrupt_disable_sem	\n"
		"js	_panic_disable_overflow	\n"
#if (DEBUG_INT_DISABLE)
		"pushl _ints_off_str		\n"
		"call _printf			\n"
		"addl $4, %esp			\n"
#endif
	);
}

// FIXME: This method of calling "PANIC" will leave the stack fubared.
static inline void	enable(void)
{
	__asm__ __volatile__
	(
		"lock				\n"
		"decl	_interrupt_disable_sem	\n"
		"js	_panic_enable_overflow	\n"
		"jnz	1f			\n"
		"sti				\n"
#if (DEBUG_INT_DISABLE)
		"pushl _ints_on_str		\n"
		"call _printf			\n"
		"addl $4, %esp			\n"
#endif
		"1:				\n"
	);
}

static inline int	spinlock_test_and_set(int new_value, spinlock *lock_ptr)
{
	register int	ret;
	int	*plock = &(lock_ptr->lock);

	__asm__ __volatile__
	(
		"movl	%1, %%eax		\n"
		"movl	%2, %%edx		\n"
		"lock				\n"
		"xchgl %%eax, (%%edx)		\n"
		"movl	%%eax, %0		\n"	// FIXME: this line is not necessary...
		: /* output */ "=a" (ret)
		: /* input */ "m" (new_value), "m" (plock)
	);

	return	ret;
}

static inline void	spinlock_acquire(spinlock *lock_ptr)
{
	disable();
//	printf("spinlock_acquire: %p (%d) %s\n", lock_ptr, lock_ptr->lock, lock_ptr->name ? lock_ptr->name : "??");
	while (spinlock_test_and_set(1, lock_ptr));
//	printf("interrupt disable depth = %d\n", interrupt_disable_sem);
}

static inline void	spinlock_release(spinlock *lock_ptr)
{
//	printf("spinlock_release: %p (%d) %s\n", lock_ptr, lock_ptr->lock, lock_ptr->name ? lock_ptr->name : "??");
// Must clear the lock flag FIRST.  If we don't, a pending IRQ may deadlock.
	lock_ptr->lock = 0;
	enable();
}

static inline void	spinlock_init(spinlock *lock_ptr, const char *name)
{
	lock_ptr->lock = 0;
	lock_ptr->name = (char*)name;
}

// Returns 'true' if the spinlock is currently locked.  Useful for ASSERTs.
static inline int	spinlock_is_locked(spinlock *lock_ptr)
{
	return lock_ptr->lock;
}

void	test_spinlocks(void);

static inline void	atomic_add64(uint64 *ptr, uint32 amt)
{
	*ptr += amt;	// FIXME: incorrect.
}
