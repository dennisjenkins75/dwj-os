/*	kernel/phys_mem.h */

#ifndef __PHYS_MEM_H__
#define __PHYS_MEM_H__

// Need definition of 'memory_map_t' from the multi-boot spec.
#include "kernel/multiboot.h"

// Size of physical memory page.  MMU allocation unit.
#define PAGE_BITS		12
#define PTE_BITS		10
#define PDE_BITS		10

#define PAGE_SIZE		(1 << PAGE_BITS)
#define PAGE_MASK		(~(PAGE_SIZE - 1))

#define PTE_SIZE		(1 << PTE_BITS)
#define PTE_MEM_MASK		(~(PTE_SIZE - 1))

#define PDIR_SIZE		(1 << (PTE_BITS + PAGE_BITS))
#define PDIR_MASK		(~(PDIR_SIZE - 1))

#define MAX_PAGES		(1 << 20)
#define ADDR_TO_PDE_SLOT(x)	((uint32)(x) >> (PTE_BITS + PAGE_BITS))
#define ADDR_TO_PTE_SLOT(x)	(((uint32)(x) >> PAGE_BITS) & ~PTE_MEM_MASK)
#define PDE_SLOT_TO_ADDR(x)	((uint32)(x) << (PTE_BITS + PAGE_BITS))
#define PTE_SLOT_TO_ADDR(x)	((uint32)(x) << PAGE_BITS)
#define PDE_PTE_TO_ADDR(d,t)	(PDE_SLOT_TO_ADDR(d) | PTE_SLOT_TO_ADDR(t))


#define PAGE_BASE(x)		((uint32)(x) & PAGE_MASK)
#define PAGE_OF(x)		((uint32)(x) >> PAGE_BITS)
#define PAGE_AFTER(x)		PAGE_OF((uint32)(x) + PAGE_SIZE - 1)

#define IS_PAGE_ALIGNED(x)	(!((uint32)(x) & ~PAGE_MASK))
#define IS_PDIR_ALIGNED(x)	(!((uint32)(x) & ~PDIR_MASK))

// Bit flags used in page table entries.
#define PTE_4M_PAGE	0x080
#define PTE_4K_PAGE	0x000

#define PTE_USERMODE	0x004
#define PTE_SUPERVISOR	0x000

#define PTE_RW		0x002
#define	PTE_RO		0x000

#define PTE_PRESENT	0x001
#define PTE_NOT_PRESENT	0x000

#define PTE_DIRTY	0x040
#define PTE_ACCESSED	0x020
#define PTE_SYSTEM	0xe00

// According to the Intel spec, the CPU will let ring-0 code
// read and write all present pages, even if they are marked read-only.
#define	PTE_KCODE	(PTE_RO | PTE_SUPERVISOR | PTE_PRESENT)
#define	PTE_KDATA	(PTE_RW | PTE_SUPERVISOR | PTE_PRESENT)
#define	PTE_UCODE	(PTE_RO | PTE_USERMODE | PTE_PRESENT)
#define	PTE_UDATA	(PTE_RW | PTE_USERMODE | PTE_PRESENT)

#define PTE_ALL_FLAGS	0x007

// Addtional flags that can be passed to "vmm_map_pages"
#define VMM_PHYS_REAL	0x80000000
#define VMM_REMAP_OK	0x40000000
#define VMM_SKIP_MAPPED	0x20000000

// Bits that are valid to pass to "vmm_map_pages" as flags.
#define VMM_MAP_VALID_FLAGS	(PTE_ALL_FLAGS | VMM_PHYS_REAL | VMM_REMAP_OK | VMM_SKIP_MAPPED)

// Flags applied to the i686 CR0 register.
#define CR0_PG_MASK 	(1 << 31)
#define CR0_WP_MASK	(1 << 16)

#define BIOS_PAGE_COUNT	((1024 * 1024) / PAGE_SIZE)

struct vmm_stats
{
	uint32	pmm_free_pages;
};

// This structure appears at the beginning of every free physical page.
typedef struct	pmm_list_hdr
{
	struct pmm_list_hdr	*next;		// Next page of the same size.
	uint32			size;		// Size in bytes.
} pmm_list_hdr;

// Physical address of the next free 4k physical page.
extern void	*gp_next_free_4k_page;
extern uint32	gp_total_free_4k_pages;


// VIRTUAL address of the physical kernel page directory.
extern uint32	*gp_kernel_page_dir;

extern void	vmm_init(const struct phys_multiboot_info *mbi);
extern void	pm_dump_memmap(void);

// Flags passed to 'pmm_alloc()':
//#define PMM_CANFAIL	(1 << 0)
//#define PMM_BIOS	(1 << 1)

// Returns physical address of a free physical RAM page.
extern void*	pmm_get_page(void);

// Places the physical page back into the linked list of available pages.
extern void	pmm_free_page(void *physical);

extern void	vmm_get_stats(struct vmm_stats *stats);

// Maps a range of physical page to a (page aligned) virtual address.
extern uint32	vmm_map_pages(void *virtual, void *physical, uint32 count, uint32 flags);

// Unmaps a range of virutal pages.
extern void	vmm_unmap_pages(void *virtual, uint32 count);

// Diagnostic function.
extern void	vmm_debug_virt_addr(const void *virtual);

// Claim all physical memory below 1M.  Claim pages used by .setup
extern void	vmm_init_cleanup(void);

// pagefault.c
extern int	vmm_page_fault(struct regs *r, void *cr2_value);

// setup_vmm.c
extern void	ATTR_SETUP_TEXT _setup_init_paging(const struct phys_multiboot_info *mbi);

// These are declared in kernel/start.S and filled in by the linker script.
// They represent the virtual memory locations of the start and end of each
// section.
extern const unsigned long _setup_start, _setup_end;
extern const unsigned long _text_start, _text_end;
extern const unsigned long _data_start, _data_end;
extern const unsigned long _bss_start, _bss_end;

// Virtual address that "setup_vmm.c" uses to temporarily map 4M pages to
// when initializing physical RAM.
extern const unsigned long _kernel_vmm_setup;

// Virtual address the kernel was configured for.  Set by linker script.
extern const unsigned long _kernel_virtual;

// Virtual address of where the kernel heap will start.  Set by the linker script.
extern const unsigned long _kernel_heap_start;
extern const unsigned long _kernel_heap_end;

// Virtual address of where we remap the VGA console to.
extern const unsigned long _kernel_console_start;

// Virtual address of where the kernel stack is, and its size.
extern const unsigned long _kernel_stack_start;
extern const unsigned long _kernel_stack_size;

// Virtual address of where we map the kernel page tables to.
extern const unsigned long _kernel_ptbl_start;

// Virtual address of pool of virtual pages that kernel can use to
// temporarily map a physical page.
extern const unsigned long _kernel_temp_vpages_start;

// Virtual address of where we begin mapping kernel modules to.
extern const unsigned long _kernel_mod_map_start;
extern const unsigned long _kernel_mod_map_end;

// Computed when initializing the temp page tables.
// Store the physical addresses and sizes of the kernel.
extern unsigned long	g_code_start;   // physical address.
extern unsigned long	g_code_pages;   // # of memory pages.
extern unsigned long	g_data_start;
extern unsigned long	g_data_pages;
extern unsigned long	g_modules_start;	// GRUB loaded modules (all of them)
extern unsigned long	g_module_pages;

extern void*		g_pVastMapAddr;

#endif // __PHYS_MEM_H__
