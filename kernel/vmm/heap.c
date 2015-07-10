/*	kernel/heap.c

	Implements the kernel's generic heap. (ie, kmalloc, kfree).
	Physical pages are not pre-allocated. They are allocated in
	chunks during page not-found page faults.

	Uses the horrible "first fit" placement method.

	This heap is designed as a place holder.  Hopefully it will be
	rewritten someday.
*/

#include "kernel/kernel.h"
#include "kernel/vmm/heap_block.h"

// Pointer to next virtual address to assign to next physical page added to our heap.
// It grows upwards until it hits top of memory.
static void *heap_start = (void*)&_kernel_heap_start;
static void *heap_end = (void*)&_kernel_heap_end;

static struct block_t	*free_list = NULL;
static struct block_t	*alloc_list = NULL;

static spinlock		heap_lock = INIT_SPINLOCK("heap");

#if (HEAP_TRACK)
static uint32		alloc_seq_id = 0;
#endif

#define MIN_BLOCK_SIZE	(sizeof(struct block_t) + sizeof(uint32))

// Given a block pointer, return the rear_guard pointer.
// The rear-gaurd is HEAP_ALLOC_GRANULARITY bytes large, but
// is always a multiple of sizeof(uint32) (ie, 4).
static inline uint32*	rear_guard_ptr(const struct block_t *block)
{
	return (uint32*)((uint8*)block + block->size - HEAP_ALLOC_GRANULARITY);
}

void	walk_list(struct block_t *hdr, const char *name)
{
	int	i = 0;

	for (i = 0; hdr && (i < 5); hdr = hdr->next, i++)
	{
		kdebug (DEBUG_INFO, FAC_HEAP, "heap_dump: %5s: hdr=%p: size=%p, next=%p, prev=%p\n", name, hdr, hdr->size, hdr->next, hdr->prev);

		if (hdr == hdr->next)
		{
			kdebug(DEBUG_FATAL, FAC_HEAP, "heap_dump: ERROR!  hdr = hdr->next\n");
			Halt();
		}
	}
}

void	heap_dump(void)
{
	walk_list(free_list, "free");
	walk_list(alloc_list, "alloc");
}

#if (HEAP_TRACK)
void	*__kmalloc(uint32 bytes, uint32 flags, const char *file, int line, const char *func)
#else
void	*__kmalloc(uint32 bytes, uint32 flags)
#endif
{
	struct block_t *block = NULL;	// The block that we allocate (with header + footer).
	struct block_t *next = NULL;	// Block after ours, if we split a larger free-block.
	void *result = NULL;		// What we return to the caller.
	uint32	*rear_guard = NULL;	// Where we put our guard magic value (to detect over-runs).
	uint32	req_bytes = 0;		// How many bytes for user + rear guard.
	uint32	total_bytes = 0;	// How much we need to carve out of free-space.
	int	i = 0;			// Generic loop variable.

	req_bytes = ROUND_UP(bytes, HEAP_ALLOC_GRANULARITY);

	total_bytes = ROUND_UP(sizeof(struct block_t) + req_bytes + sizeof(uint32), HEAP_ALLOC_GRANULARITY);

#if (DEBUG_HEAP)
	{ uint8 attr = con_set_attr(0x0f);
	kdebug(DEBUG_DEBUG, FAC_HEAP, "kmalloc(%u,%u) rounds to %u\n", bytes, flags, total_bytes);
	con_set_attr(attr); }
#endif

	spinlock_acquire(&heap_lock);

// Walk free list looking for block big enough.
	if (free_list)
	{
		for (block = free_list; block->next && (total_bytes > block->size); block = block->next);
	}

	if ((uint32)block % HEAP_ALLOC_GRANULARITY)
	{
		kdebug(DEBUG_DEBUG, FAC_HEAP, "heap: block(%p) is not properly aligned.\n", block);
		ASSERT((uint32)block % HEAP_ALLOC_GRANULARITY == 0);
	}

// Are we out of heap space?
	if (!block)
	{
		if (flags & HEAP_FAILOK)
		{
			spinlock_release(&heap_lock);
			kdebug(DEBUG_WARN, FAC_HEAP, "heap: malloc(%d) failed.  Returning NULL\n", bytes);
			return NULL;
		}

		PANIC2("heap: malloc(%d) failed.", bytes);
	}

// Did we find a block bigger than what we needed?  If so, split it.
	if (block->size > total_bytes)
	{
#if (DEBUG_HEAP)
		kdebug(DEBUG_DEBUG, FAC_HEAP, "heap: found block of %d at %p for request of %d bytes\n", block->size, block, total_bytes);
#endif

// Do we have enough space left over for future allocation?
		if (block->size - total_bytes > MIN_BLOCK_SIZE)
		{
#if (DEBUG_HEAP)
			kdebug(DEBUG_DEBUG, FAC_HEAP, "Splitting block %p for %d bytes\n", block, total_bytes);
#endif

			next = (struct block_t*)((uint8*)block + total_bytes);
			next->size = block->size - total_bytes;
			next->next = block->next;
			next->prev = block->prev;
			next->guard = MALLOC_GUARD_MAGIC;

			block->next = next;
			block->size = total_bytes;
		}
	}

// At this point we know that we are operating on "block".  We've already split (if possible) a free block.
	if (free_list == block)
	{
		free_list = block->next;
		ASSERT_HEAP(free_list);
	}

	result = (void*)((uint8*)block + sizeof(struct block_t));
	ASSERT((uint32)result % HEAP_ALLOC_GRANULARITY == 0);

// Fill user memory.
	memset(result, MALLOC_FILL, req_bytes);

// FIll our guard bytes (so we can detect over-run / under-run later).
	rear_guard = rear_guard_ptr(block);
	for (i = HEAP_ALLOC_GRANULARITY; i; *rear_guard = MALLOC_GUARD_MAGIC, rear_guard++, i -= 4);
	block->guard = MALLOC_GUARD_MAGIC;

// Place "block" into the alloc list.
	if (!alloc_list)
	{
		block->next = NULL;
		block->prev = NULL;
		alloc_list = block;
	}
	else
	{
		struct block_t *node;

		// Stop when we found the alloc'd item that is just past out current one.
		for (node = alloc_list; node->next; node = node->next)
		{
			if ((node < block) && (node->next > block))
			{
				break;
			}
		}

		if (node)
		{
			block->next = node->next;
			block->prev = node;

			node->next = block;

			if (block->next)
			{
				block->next->prev = block;
			}
		}
		else
		{
			PANIC1("kmalloc(): internal error, can't insert into alloc-list.");
		}
	}

#if (HEAP_TRACK)
	block->seq_id = alloc_seq_id++;
	block->file = file;
	block->line = line;
	block->func = func;
#endif

#if (HEAP_REPLAY)
	kdebug(DEBUG_DEBUG, FAC_HEAP, "heap-replay: replay_list[%d] = kmalloc(%d,%d);\n", block->seq_id, bytes, flags);
#endif

	spinlock_release(&heap_lock);


	return result;
}

// If possible, merge 'a' and 'b'.  Assume 'a<b'.  Returns 'a' if they merge,
// return 'b' if not.
struct block_t *	heap_merge(struct block_t *a, struct block_t *b)
{
#if (DEBUG_HEAP)
	kdebug(DEBUG_DEBUG, FAC_HEAP, "heap: heap_merge(%p, %p) - trying.\n", a, b);
#endif

	ASSERT(a);
	ASSERT(b);
	ASSERT(a < b);

// Are the nodes candidates to merge?
	if ((a->next == b) && (b->prev == a) && ((uint8*)a + a->size == (uint8*)b))
	{
#if (DEBUG_HEAP)
		kdebug (DEBUG_DEBUG, FAC_HEAP, "heap: merging %p and %p\n", a, b);
#endif

		a->size += b->size;
		a->next = b->next;
		if (a->next)
		{
			a->next->prev = a;
		}

		return a;
	}

	return b;
}


#if (HEAP_TRACK)
void	__kfree(void *ptr, const char *file, int line, const char *func)
#else
void	__kfree(void *ptr)
#endif
{
	struct block_t *hdr = NULL;
	struct block_t *tmp = NULL;
	uint32		*rear_guard = NULL;
	int		i = 0;

#if (DEBUG_HEAP)
	{ uint8 attr = con_set_attr(0x0f);
	kdebug(DEBUG_DEBUG, FAC_HEAP, "kfree(%p, %s, %d, %s)\n", ptr, file, line, func);
	con_set_attr(attr); }
#endif

// Do some sanity checks.
	if ((ptr < heap_start) || (ptr > heap_end))
	{
		PANIC2("kfree(%p) not in valid heap range!", ptr);
	}

	if ((uint32)ptr % HEAP_ALLOC_GRANULARITY)
	{
		PANIC2("kfree(%p) not properly aligned!", ptr);
	}

	hdr = (struct block_t*)ptr - 1;

	if ((void*)hdr < (void*)heap_start)
	{
		PANIC3("kfree(%p) block header(%p) not on heap!\n", ptr, hdr);
	}

	if (hdr->guard != MALLOC_GUARD_MAGIC)
	{
		PANIC3("kfree(%p) block header(%p) corrupt!\n", ptr, hdr);
	}

	rear_guard = rear_guard_ptr(hdr);
	for (i = HEAP_ALLOC_GRANULARITY; i; i -= 4, rear_guard++)
	{
		if (*rear_guard != MALLOC_GUARD_MAGIC)
		{
			kdebug_mem_dump(DEBUG_FATAL, FAC_HEAP, (void*)&_kernel_heap_start, 128);
			PANIC2("kfree(%p) user-chunk was over-run.\n", ptr);
		}
	}

#if (DEBUG_HEAP)
	kdebug (DEBUG_DEBUG, FAC_HEAP, "kfree(%p) size was %d\n", ptr, hdr->size);
#endif

#if (HEAP_REPLAY)
	kdebug (DEBUG_DEBUG, FAC_HEAP, "heap-replay: kfree(replay_list[%d]); replay_list[%d] = NULL;\n", hdr->seq_id, hdr->seq_id);
#endif

	memset(ptr, 0xce, hdr->size - sizeof(struct block_t));

// Remove from alloc list.
	for (tmp = alloc_list; tmp && (tmp != hdr); tmp = tmp->next);

	if (alloc_list == tmp)
	{
		alloc_list = tmp->next;
	}
	else
	{
		ASSERT (tmp->prev);	// every node on a list MUST have a 'prev' node if it is not the head node.

		tmp->prev->next = tmp->next;

		if (tmp->next)
		{
			tmp->next->prev = tmp->prev;
		}
	}

// Add node back into free-list.

	if ((hdr < free_list) || !free_list)
	{
// Insert at beginning of list.
		hdr->next = free_list;
		hdr->prev = NULL;
		free_list = hdr;
		tmp = hdr;

		ASSERT_HEAP(free_list);

		if (hdr->next)
		{
			hdr->next->prev = hdr;
		}
	}
	else
	{
// Insert in middle of end of list.
		for (tmp = free_list; tmp; tmp = tmp->next)
		{
			if ((tmp < hdr) && ((tmp->next > hdr) || !tmp->next))
			{
				hdr->next = tmp->next;
				hdr->prev = tmp;

				if (tmp->next)
				{
					tmp->next->prev = hdr;
				}
				tmp->next = hdr;

				break;
			}
		}
	}

// Try to merge "hdr->prev" and "hdr", if they are contiguous.
	if (hdr->prev)
	{
		hdr = heap_merge(hdr->prev, hdr);
	}

	if (hdr->next)
	{
		heap_merge(hdr, hdr->next);
	}

#if (DEBUG_HEAP)
	kdebug (DEBUG_DEBUG, FAC_HEAP, "kfree(%p) done.\n", ptr);
#endif
}

int	heap_count_nodes(struct block_t *list)
{
	int	count = 0;

	for (; list; list = list->next, count++)
	{
		if (list == list->next)
		{
			PANIC2("circular heap list: %p\n", list);
		}
	}
	return count;
}

// Test: That we can allocate a block, free it, allocate a new one of
// different size and get the same address as the first allocation.
// On entry: empty heap.
// On exit: empty heap.
void	test_heap_1(void)
{
	void	*a = NULL;
	void	*b = NULL;

	a = kmalloc(44, 0);
	kfree(a);
	b = kmalloc(129, 0);
	kfree(b);

// post-conditions.
	ASSERT(a == b);
}

// Test: Allocate 2 blocks, free first, then free second.
void	test_heap_2(void)
{
	void *a = NULL;
	void *b = NULL;

	a = kmalloc(8, 0);
	b = kmalloc(8, 0);

	kfree(a);
	kfree(b);

	a = kmalloc(8, 0);
	b = kmalloc(8, 0);

	kfree(b);
	kfree(a);
}

// Allocate large array of varying sizes.
// Free all odd nodes.
// Free all even nodes.
void	test_heap_3(void)
{
#define		TEST3_COUNT	8
	void*	array[TEST3_COUNT];
	int	i;

	for (i = 0; i < TEST3_COUNT; i++)
	{
		array[i] = kmalloc(i * PAGE_SIZE, 0);
	}

	for (i = 1; i < TEST3_COUNT; i += 2)
	{
		kfree(array[i]);
		array[i] = NULL;
	}

	for (i = 0; i < TEST3_COUNT; i += 2)
	{
		kfree(array[i]);
		array[i] = NULL;
	}
}

// test re-use of a block big enough for the current allocation, bu not big enough to split.
void	test_heap_4(void)
{
	void *array[4];

	array[0] = kmalloc(68, 0);
	array[1] = kmalloc(68, 0);
	array[2] = kmalloc(68, 0);

	kfree(array[1]);
	array[3] = kmalloc(64, 0);

	ASSERT(array[1] == array[3]);

// if we make it here, the test passed.
	kfree(array[0]);
	kfree(array[2]);
	kfree(array[3]);
}

typedef void (*test_func)(void);
static test_func tests[] =
{
	test_heap_1,
	test_heap_2,
	test_heap_3,
	test_heap_4,
	NULL
};

void	test_heap(void)
{
	int	i;

	ASSERT(heap_count_nodes(alloc_list) == 0);
	ASSERT(heap_count_nodes(free_list) == 1);

	for (i = 0; tests[i]; i++)
	{
//		printf("heap:\ttest %d\n", i);

		tests[i]();
//		heap_dump();

//		printf("\t\talloc_list\n");
		ASSERT(heap_count_nodes(alloc_list) == 0);
//		printf("\t\tfree_list\n");
		ASSERT(heap_count_nodes(free_list) == 1);

	}

//	printf("heap: test over.\n");
//	Halt();
}

void	heap_init(void)
{
	uint32	heap_bytes = (uint32)&_kernel_heap_end - (uint32)&_kernel_heap_start;

	if (sizeof(struct block_t) % HEAP_ALLOC_GRANULARITY)
	{
		PANIC3("heap: sizeof(struct block_t) (*%d) not a multiple of %d\n", sizeof(struct block_t), HEAP_ALLOC_GRANULARITY);
	}

	corehelp.heap_free_list_ptr = (uint32)&free_list;
	corehelp.heap_alloc_list_ptr = (uint32)&alloc_list;

// Allocate first block (to hold empty heap).
	vmm_map_pages(heap_start, NULL, HEAP_GROW_PAGES, PTE_KDATA);

// Create initial linked lists of blocks.
	free_list = (struct block_t*)heap_start;
	alloc_list = NULL;

// Initialize list of free blocks.
	memset(free_list, 0, sizeof(struct block_t));
	free_list->size = heap_bytes - sizeof(struct block_t);
	free_list->next = NULL;

	printf("heap: %d K available.\n", heap_bytes / 1024);
	kdebug(DEBUG_INFO, FAC_HEAP, "heap: %d K available.\n", heap_bytes / 1024);

	test_heap();
}

// Called at interrupt context while handling a page fault while
// accessing the kernel's heap.  This routine is used to fault
// in additional pages into the heap.
void	heap_grow(struct regs *r, void *cr2_value)
{
	void	*virt = (void*)((uint32)cr2_value & HEAP_ALLOC_MASK);
	uint32	count = HEAP_GROW_PAGES;

	if (!r || (cr2_value < (void*)&_kernel_heap_start) || (cr2_value >= (void*)&_kernel_heap_end))
	{
		PANIC2("heap: heap_grow(%p) invalid virtual address!\n", cr2_value);
	}

// Are we near the end of the heap (and need to trim count)?
	if ((void*)((uint32)virt + count * PAGE_SIZE) > (void*)&_kernel_heap_end)
	{
		count = ((uint32)&_kernel_heap_end - (uint32)virt) / PAGE_SIZE;
	}

	if (!count)
	{
		PANIC1("heap: not enough memory to grow.\n");
	}

	count = vmm_map_pages(virt, NULL, count, PTE_KDATA | VMM_SKIP_MAPPED);

#if (DEBUG_HEAP)
	kdebug(DEBUG_DEBUG, FAC_HEAP, "heap: added %d pages, %d free\n", count, gp_total_free_4k_pages);
#endif
}
