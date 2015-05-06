/*	kernel/heap.h	*/

#ifndef __HEAP_H__
#define __HEAP_H__

// Various bit-flags that can be passed as 'flags' to kmalloc().
// Bottom bit affects alignment of the result.
#define	HEAP_FAILOK	(1 << 1)

#define HEAP_ALLOC_MASK (~(HEAP_GROW_PAGES * PAGE_SIZE -1))

void	mem_dump(const void *addr, uint32 bytes);

void	heap_init(void);
void	heap_grow(struct regs *r, void *cr2_value);

#if (HEAP_TRACK)
	void	__kfree(void *ptr, const char *file, int line, const char *func);
	void    *__kmalloc(uint32 bytes, uint32 flags, const char *file, int line, const char *func);

	#define kfree(x) __kfree(x,__FILE__,__LINE__,__FUNCTION__)
	#define kmalloc(x,f) __kmalloc(x,f,__FILE__,__LINE__,__FUNCTION__)
#else
	void	__kfree(void *ptr);
	void    *__kmalloc(uint32 bytes, uint32 flags);
	#define kfree(x) __kfree(x)
	#define kmalloc(x,f) __kmalloc(x,f)
#endif

static inline int is_heap_ptr(const void *x)
{
	return (x >= (const void*)&_kernel_heap_start) && (x < (const void*)&_kernel_heap_end);
}


#endif	// _HEAP_H__
