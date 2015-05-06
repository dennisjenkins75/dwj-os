/*	kernel/kernel/corehelp.h

	Defines a structure that is allocated in the kernel's .data section.
	This structure holds pointers to important variables and is used
	by the external core memory debugger to dump important kernel
	things, like the process list, heap, wait lists, etc...
*/

#define CORE_HELP_MAGIC1	0x4f8d93cb
#define CORE_HELP_MAGIC2	0x2e3d4c5b

struct	corehelp
{
	uint32		magic1;
	uint32		size;
	uint32		magic2;

	uint32		cr3;			// kernel's CR3 value.
	uint32		task_list_ptr_ptr;	// pointer to task_list
	uint32		task_current_ptr_ptr;	// pointer to current
	uint32		heap_free_list_ptr;	// &free_list
	uint32		heap_alloc_list_ptr;	// &alloc_list
	uint32		obj_array_ptr_ptr;	// pointer to _handle_array
	uint32		obj_array_size_ptr;	// pointer to _handle_array_size
};

#if defined (BUILDING_KERNEL)
extern struct corehelp corehelp;
#endif
