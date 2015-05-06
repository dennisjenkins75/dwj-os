/*	tools/core-debugger/heap.c	*/

#include "core-debugger.h"
#include "kernel/vmm/heap_block.h"

static void	DumpBlock(const char *type, struct block_t *blk)
{
	uint32 size = (ReadDword((uint32)&blk->size) - sizeof(*blk) - sizeof(uint32)) & ~7;
	uint32 seq_id = ReadDword((uint32)&blk->seq_id);
	uint32 line = ReadDword((uint32)&blk->line);
	char *file = DupString(ReadDword((uint32)&blk->file));
	char *func = DupString(ReadDword((uint32)&blk->func));
	uint32 ptr = (uint32)blk + sizeof(*blk);

	printf("%s %08x = seq:%5d, size:%5d, %-30.30s (%4d) %s\n",
		type, ptr, seq_id, size, func, line, basename(file));

	free(file);
	free(func);
}

void	DumpHeap(void)
{
	uint32	free_list_ptr = *(uint32*)(&corehelp->heap_free_list_ptr);
	uint32	alloc_list_ptr = *(uint32*)(&corehelp->heap_alloc_list_ptr);
	struct block_t *free_blk = (struct block_t*)ReadDword(free_list_ptr);
	struct block_t *alloc_blk = (struct block_t*)ReadDword(alloc_list_ptr);

	printf("free_list  = %08x -> %08x\n", free_list_ptr, (uint32)free_blk);
	printf("alloc_list = %08x -> %08x\n", alloc_list_ptr, (uint32)alloc_blk);

	while (1)
	{
		if (free_blk && (free_blk < alloc_blk))
		{
			DumpBlock("free ", free_blk);
			free_blk = (struct block_t*)ReadDword((uint32)&free_blk->next);
		}
		else if (alloc_blk)
		{
			DumpBlock("alloc", alloc_blk);
			alloc_blk = (struct block_t*)ReadDword((uint32)&alloc_blk->next);
		}
		else
		{
			break;
		}
	}



//	DumpMemory(0xf1000000, 16384);

}
