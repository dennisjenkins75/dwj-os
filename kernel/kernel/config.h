/*	kernel/config.h		*/

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define DEBUG_LOG_TO_LPT	1
#define LPT_BASE		0x378

#define DEBUG_LOG_BOCHS_E9	1

#define DEBUG_SETUP_PAGING	0

#define DEBUG_PMM_MAP_UNMAP	0

#define DEBUG_TIMER_TICK	0

#define DEBUG_INT_DISABLE	0

// Memory manager needs a cache of available virtual addresses for making temporary mappings.
// This array is guarded by a spinlock and has a small number of entries.
#define VMM_TEMP_PAGES		16

// All heap allocations will be rounded up to the nearest multiple of this:
#define HEAP_ALLOC_GRANULARITY	8

// Do extra heap integrity checks on every alloc/free.
#define DEBUG_HEAP		1

// Should the heap keep track of who allocated each block (16 bytes overhead per-kmalloc).
#define HEAP_TRACK		1

// Causes the kmalloc/kfree to emit the data necessary to "replay" the allocation sequence.
#define HEAP_REPLAY		0

// Grow the heap by this many pages everytime it needs to grow.
// MUST be a power of 2!
#define HEAP_GROW_PAGES		8

// Number of keystrokes to buffer in keyboard driver.
#define KBD_BUFFER_SIZE		128

// Number of time slices each thread gets in its quantum
#define DEFAULT_THREAD_QUANTUM	100

#define FIXME()  do {} while (0)
//#define FIXME()  PANIC4("FIXME: %s, %d, %s", __FILE__, __LINE__, __FUNCTION__)

#define TASK_KSTACK_SIZE	4096

#define INITIAL_OBJECT_ARRAY_SIZE	4096

#define MAX_QUANTUM		100

#endif	// __CONFIG_H__
