/*	kernel/vmm/heap_block.h

	Defines heap structures.  Done here so that the debugger can also
	include this header file and not get the rest of the kernel junk.
*/

// Newly allocated memory is filled with this value:
#define MALLOC_FILL	0xcc

// Guard uint32 before and after the block have this value.
// Just some random bits with the high-bit set.  Hopefully won't
// ever correspond to a valid memory address or string (note
// the LSB bit is set, so it treated as a pointer it will
// ALWAYS be unaligned).
#define MALLOC_GUARD_MAGIC	0xbdbdbdbd

/* The free_list has this structure at the beginning of each node.
   The alloc_list has this strucutre at the beginning of each block,
   but the pointer returned to the caller points to the data after
   the block. */
struct block_t
{
	uint32		size;		// Total size of this block, including header, data and rear guard.
	struct block_t	*next;		//
	struct block_t	*prev;		//

#if (HEAP_TRACK)
	uint32		seq_id;		// Which allocation this was.
	const char 	*file;		// __FILE__
	int 		line;		// __LINE__
	const char	*func;		// __FUNCTION__
#endif

	uint32		guard;		// MALLOC_GUARD, for detecting under-run.
};
