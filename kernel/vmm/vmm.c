/*	kernel/phys_mem.c */

#include "kernel/kernel.h"

/* Note: The following symbols represent the physical memory layout
   of the system memory.  The sections of memory are in the same order
   that these variables are declared.  Memory after the last section is
   considered free memory that can be allocated.
*/

uint32	g_setup_start;
uint32	g_setup_pages;
uint32	g_code_start;   // physical address of where code segment is.
uint32	g_code_pages;   // # of memory pages of kernel code.
uint32	g_data_start;
uint32	g_data_pages;
uint32	g_modules_start;
uint32	g_module_pages;
void	*g_pVastMapAddr = NULL;

void	*gp_next_free_4k_page = NULL;
uint32	gp_total_free_4k_pages = 0;
uint32	*gp_kernel_page_dir = NULL;

static const char *pte_flag_chars = "sss00da00uwp";

static void	*temp_vpages[VMM_TEMP_PAGES];
static int	temp_vpages_idx = 0;
static spinlock	temp_vpages_lock = INIT_SPINLOCK("temp_vpages");

void	vmm_get_stats(struct vmm_stats *stats)
{
	stats->pmm_free_pages = gp_total_free_4k_pages;
}

// Locked when searching or updating the active page tables.
//static spinlock	kernel_pde_lock = INIT_SPINLOCK("pde");

static void	*borrow_vpage(void)
{
	void	*ret = NULL;

	spinlock_acquire(&temp_vpages_lock);

	if (temp_vpages_idx >= VMM_TEMP_PAGES)
	{
		PANIC2("vmm: Exhausted %d temp_vpages.\n", temp_vpages_idx);
	}

	ret = temp_vpages[temp_vpages_idx];
	temp_vpages[temp_vpages_idx] = 0;
	temp_vpages_idx++;

	spinlock_release(&temp_vpages_lock);

	{
		int pde_slot = ADDR_TO_PDE_SLOT(ret);
		int pte_slot = ADDR_TO_PTE_SLOT(ret);
		uint32 ptbl = gp_kernel_page_dir[pde_slot];

		if (!ptbl)
		{
int i;
			printf("borrowing: %p  [%03x,%03x] cr3[%03x] = %p\n", ret, pde_slot, pte_slot, pde_slot, ptbl);
for (i = 0; i < 0x400; i++) if ( gp_kernel_page_dir[i] ) printf("gp_kernel_page_dir[%03x] = %p\n", i, gp_kernel_page_dir[i]);
			Halt();
		}
	}

	return ret;
}

static void	return_vpage(void *vpage)
{
	spinlock_acquire(&temp_vpages_lock);

	if (!temp_vpages_idx)
	{
		PANIC2("vmm: Attempt to return vpage when not acquired: %p\n", vpage);
	}

	temp_vpages[--temp_vpages_idx] = vpage;

	spinlock_release(&temp_vpages_lock);
}

void	vmm_debug_virt_addr(const void *virtual)
{
	int	pde_slot = ADDR_TO_PDE_SLOT(virtual);
	int	pte_slot = ADDR_TO_PTE_SLOT(virtual);
	uint32	ptbl = gp_kernel_page_dir[pde_slot] & PAGE_MASK;
	uint32	physical = 0;
	void	*temp = NULL;

	if (!ptbl)
	{
		printf ("virt:%p, (pde,pte:%03x,%03x) ptbl:--NULL--,\n",
			virtual, pde_slot, pte_slot);
		return;
	}

// temporarily map ptbl to temp vpage.
	temp = borrow_vpage();
	vmm_map_pages(temp, (void*)ptbl, 1, PTE_KDATA | VMM_PHYS_REAL);
	physical = ((uint32*)temp)[pte_slot] & PAGE_MASK;
	return_vpage(temp);

	printf ("virt:%p, (pde,pte:%03x,%03x) ptbl:%p, phys:%p\n",
		virtual, pde_slot, pte_slot, ptbl, physical);

	return;
}

// Dump all mapped virtual memory pages table entreis between these two addresses.
void	vmm_dump_page_tables(const void *virt_start, const void *virt_end)
{
	uint32	flags;
	uint32	*ptbl_phys;
	uint32	*ptbl_virt;
	uint32	paddr;
	uint32	vaddr;
	char	flag_str[13];
	int	first_pde = ADDR_TO_PDE_SLOT(virt_start);
	int	last_pde = ADDR_TO_PDE_SLOT(virt_end);
	int	first_pte = 0;
	int	last_pte = 0;
	int	pde = 0;
	int	pte = 0;
	int	f;

	for (pde = first_pde; pde <= last_pde; pde++)
	{
		if (!(gp_kernel_page_dir[pde] & PTE_PRESENT)) continue;

		flags = gp_kernel_page_dir[pde] & ~PAGE_MASK;
		ptbl_phys = (uint32*)(gp_kernel_page_dir[pde] & PAGE_MASK);
		ptbl_virt = (uint32*)((uint32)&_kernel_ptbl_start + (uint32)(pde << 12));
		vaddr = (pde << 22);

		printf("cr3[%03x] (virt:%p) = %08x, %03x\n", pde, vaddr, ptbl_phys, flags);

		first_pte = (pde == first_pde) ? ADDR_TO_PTE_SLOT(virt_start) : 0;
		last_pte = (pde == last_pde) ? ADDR_TO_PTE_SLOT(virt_end) : 1023;

		for (pte = first_pte; pte <= last_pte; pte++)
		{
			if (!(ptbl_virt[pte] & PTE_PRESENT)) continue;

			flags = ptbl_virt[pte] & ~PAGE_MASK;
			paddr = ptbl_virt[pte] & PAGE_MASK;
			vaddr = (pde << 22) | (pte << 12);

			for (f = 0; f < 12; f++)
			{
				flag_str[f] = flags & (1 << (11 - f)) ? pte_flag_chars[f] : '-';
			}
			flag_str[f] = 0;

			printf("\t%08x [%03x,%03x] = p:%08x, %03x (%s)\n",
				vaddr, pde, pte, paddr, flags, flag_str);
		}
	}
}


// Clears 'count' pages at 'addr' to all zero.  Assumes 'addr' is page aligned.
static inline void clear_pages(void *addr, int count)
{
	__asm__ __volatile__
	(
		"cld\n"
		"movl	%0, %%edi\n"		// EDI = addr
		"movl	%1, %%ecx\n"		// ECX = page count
		"shll	%2, %%ecx\n"		// ECX = dword count
		"xorl	%%eax, %%eax\n"		// EAX = 0
		"repne\n"
		"stosl"
		: /* Outputs */
		: /* Inputs */ "m"(addr), "m"(count), "i"(PAGE_BITS - 2)
		: /* Clobbers */ "%edi", "%ecx", "%eax"
	);
}

static inline const void* virt_to_phys(const void *virt)
{
	return ((void*)virt > (void*)&_kernel_virtual) ?
		(uint8*)virt - (uint32)&_kernel_virtual :
		virt;
}

// Unlinks a physical page from the free page list.  Has to map the page
// temporarily to get the pointer for the next page in the list.  Clears
// the page out before returning it.  Page returned in NOT MAPPED.  Return
// value is raw physical address of page.

void*		pmm_get_page(void)
{
	void		*ret = NULL;
	pmm_list_hdr	*node = (pmm_list_hdr*)borrow_vpage();

	if (NULL == (ret = gp_next_free_4k_page))
	{
		PANIC1("pmm_get_page: no free pages available.\n");
		return NULL;
	}

	vmm_map_pages(node, ret, 1, PTE_KDATA | VMM_PHYS_REAL);

	gp_next_free_4k_page = node->next;
	gp_total_free_4k_pages--;

	clear_pages(node, 1);

	vmm_unmap_pages(node, 1);

	return_vpage(node);

	return ret;
}

void		pmm_free_page(void *physical)
{
	pmm_list_hdr    *node = (pmm_list_hdr*)borrow_vpage();

//	Map the page (temporarily) so that we can follow its link and clear it out.
	vmm_map_pages(node, physical, 1, PTE_KDATA | VMM_PHYS_REAL);

	clear_pages(node, 1);
	node->next = gp_next_free_4k_page;
	node->size = PAGE_SIZE;
	gp_next_free_4k_page = physical;
	gp_total_free_4k_pages++;

	vmm_unmap_pages(node, 1);
	return_vpage(node);
}

// Walks the page tables between 'virtual' and 'virtual + count * PAGE_SIZE'.
// For any page marked as 'not present', this function will grab a physical
// page and map it.  Primarily used for growing the kernel heap by
// multiple pages at a time.  Returns the number of pages actually mapped.
/*
uint32		vmm_map_pages_rand(void* virtual, uint32 count, uint32 flags)
{
	uint32	pde_slot = 0;
	uint32	pte_slot = 0;
	uint32	page_table_phys = 0;	// value as hardware sees it.
	uint32 *page_table_virt = 0;	// where we can access the innards of the table itself.
	uint32	result = 0;
	void	*physical = NULL;

#if (DEBUG_PMM_MAP_UNMAP)
	printf("vmm: map_pages_rand: virt:%p, count:%d, flags:%x\n", virtual, count, flags);
#endif

	if (!gp_kernel_page_dir)
	{
		PANIC1("gp_kernel_page_dir == NULL inside map_pages_rand().\n");
	}

	if (flags & ~(PTE_ALL_FLAGS))
	{
		PANIC2("illegal flags: %b\n", flags);
	}

	if (!IS_PAGE_ALIGNED(virtual))
	{
		PANIC2("virtual address, %p, is not page aligned.\n", virtual);
	}

	while (count)
	{
		pde_slot = ADDR_TO_PDE_SLOT(virtual);
		pte_slot = ADDR_TO_PTE_SLOT(virtual);

		page_table_phys = (gp_kernel_page_dir[pde_slot] & PAGE_MASK);

		if (!page_table_phys)
		{
// It seems that the page we want to map requires a page directory entry (ie, a page table)
// to be allocated.  So we will grab a (zeroed-out) physical page and plug it in.

			// Early assertion to help track bug better.
			// Would otherwise be caught in 'pmm_get_page' and be confusing.
			ASSERT(gp_next_free_4k_page);

			page_table_phys = (uint32)pmm_get_page();	// recursively calls map/unmap
			gp_kernel_page_dir[pde_slot] = page_table_phys | PTE_KDATA;

			InvalidatePage(gp_kernel_page_dir);	// FIXME: do we need this?
		}

// Now we need to modify the page table.  But all we have is its physical address.
// (pte_table is a physical address).  We can cheat though - it is already mapped
// due to use mapping our page directory itself as a page table.

		page_table_virt = (uint32*)((uint32)&_kernel_ptbl_start + (uint32)(pde_slot << 12));

		if (!(page_table_virt[pte_slot] & PTE_PRESENT))
		{
			physical = pmm_get_page();
			page_table_virt[pte_slot] = (uint32)physical | flags;
			InvalidatePage(virtual);
			result++;
		}

		count--;
		virtual = (void*)((uint32)virtual + PAGE_SIZE);
	}

	return result;
}
*/

/* Maps physical pages into virtual address space.  Has two methods for choosing
   physical address.  "flags" usage:

	VMM_PHYS_REAL:
		If set, function will map the physical pages from "physical"
		into the virtual range.  If virtual address is already mapped,
		function will lose old mapping or panic, depending on
		setting of 'VMM_REMAP_OK' bit.

		If not set, function will grab physical pages from the
		free-page pool.

	VMM_REMAP_OK:
		If set function will NOT panic if a virtual address is already mapped.

		If clear function will PANIC if a virtual address is already mapped.

	Lower 12 bits:
		These bits are applied directly to the page table entries.
		Generally, the caller would specify "PTE_KDATA" or "PTE_KCODE".

   Returns the number of pages actually mapped (which could be less then the number
   requested.
*/

uint32		vmm_map_pages(void* virtual, void* physical, uint32 count, uint32 flags)
{
	uint32	pde_slot = 0;
	uint32	pte_slot = 0;
	uint32	page_table_phys = 0;	// value as hardware sees it.
	uint32 *page_table_virt = 0;	// where we can access the innards of the table itself.
	uint32	pte_flags = flags & PTE_ALL_FLAGS;
	uint32	result = 0;

#if (DEBUG_PMM_MAP_UNMAP)
	printf("map_pages: virt:%p, phys:%p, count:%d, flags:%x\n", virtual, physical, count, flags);
#endif

	if (!gp_kernel_page_dir)
	{
		PANIC1("gp_kernel_page_dir == NULL inside map_pages().\n");
	}

	if (flags & ~VMM_MAP_VALID_FLAGS)
	{
		PANIC2("illegal flags: %b\n", flags);
	}

	if (!IS_PAGE_ALIGNED(virtual))
	{
		PANIC2("virtual address, %p, is not page aligned.\n", virtual);
	}

	if ((flags & VMM_PHYS_REAL) && !IS_PAGE_ALIGNED(physical))
	{
		PANIC2("physical address, %p, is not page aligned.\n", physical);
	}

	while (count)
	{
		pde_slot = ADDR_TO_PDE_SLOT(virtual);
		pte_slot = ADDR_TO_PTE_SLOT(virtual);

		page_table_phys = (gp_kernel_page_dir[pde_slot] & PAGE_MASK);

		if (!page_table_phys)
		{
// It seems that the page we want to map requires a page directory entry (ie, a page table)
// to be allocated.  So we will grab a (zeroed-out) physical page and plug it in.

			// Early assertion to help track bug better.
			// Would otherwise be caught in 'pmm_get_page' and be confusing.
			ASSERT(gp_next_free_4k_page);

			page_table_phys = (uint32)pmm_get_page();	// recursively calls map/unmap
			gp_kernel_page_dir[pde_slot] = page_table_phys | PTE_KDATA;

			InvalidatePage(gp_kernel_page_dir);	// FIXME: do we need this?
//printf("allocated new pde: %p (%d)\n", page_table_phys, pde_slot);
		}

// Now we need to modify the page table.  But all we have is its physical address.
// (pte_table is a physical address).  We can cheat though - it is already mapped
// due to use mapping our page directory itself as a page table.

		page_table_virt = (uint32*)((uint32)&_kernel_ptbl_start + (uint32)(pde_slot << 12));

		if (!(flags & VMM_REMAP_OK) && (page_table_virt[pte_slot] & PTE_PRESENT))
		{
			PANIC3("page %p already mapped to phys_addr %p!\n",
				virtual, page_table_virt[pte_slot] & PAGE_MASK);
		}

		if ((flags & VMM_SKIP_MAPPED) && (page_table_virt[pte_slot] & PTE_PRESENT))
		{
			goto skip;
		}

		if (!(flags & VMM_PHYS_REAL))
		{
			physical = pmm_get_page();
		}

		page_table_virt[pte_slot] = (uint32)physical | pte_flags;
		InvalidatePage(virtual);
		result++;

skip:
		count--;
		virtual = (void*)((uint32)virtual + PAGE_SIZE);
		physical = (void*)((uint32)physical + PAGE_SIZE);
	}

	return result;
}

void		vmm_unmap_pages(void *virtual, uint32 count)
{
	uint32	pde_slot = 0;
	uint32	pte_slot = 0;
	uint32 	page_table_phys = 0;
	uint32 *page_table_virt = NULL;

#if (DEBUG_PMM_MAP_UNMAP)
	printf("unmap_pages(virtual:%p, count:%d)\n", virtual, count);
#endif

	if (!gp_kernel_page_dir)
	{
		PANIC1("unmap_pages: gp_kernel_page_dir == NULL inside unmap_pages().\n");
	}

	if (!IS_PAGE_ALIGNED(virtual))
	{
		PANIC2("unmap_pages: virtual address, %p, is not page aligned.\n", virtual);
	}

	while (count)
	{
		pde_slot = ADDR_TO_PDE_SLOT(virtual);
		pte_slot = ADDR_TO_PTE_SLOT(virtual);

		if (!gp_kernel_page_dir[pde_slot])
		{
			PANIC2("unmap_pages: gp_kernel_page_dir[%03x] is not initialized!\n", pde_slot);
		}

		page_table_phys = (gp_kernel_page_dir[pde_slot] & PAGE_MASK);
		page_table_virt = (uint32*)((uint32)&_kernel_ptbl_start + (uint32)(pde_slot << 12));

		if (!page_table_virt[pte_slot])
		{
			PANIC2("unmap_pages: vaddr %p is not mapped!\n", virtual);
		}

		if (page_table_virt[pte_slot] & PTE_4M_PAGE)
		{
			PANIC2("unmap_pages: vaddr %p is a 4M page.\n", virtual);
		}

// Unmap the page.
		page_table_virt[pte_slot] = 0;
		InvalidatePage(virtual);

		count--;
		virtual = (void*)((uint32)virtual + PAGE_SIZE);
	}
}

#define PMM_COUNT 4
void	pmm_test(void)
{
	void	*array[PMM_COUNT];
	int	i;

	for (i = 0; i < PMM_COUNT; i++)
	{
		array[i] = pmm_get_page();
#if (DEBUG_PMM_MAP_UNMAP)
		printf("get_page() = %p (next = %p)\n", array[i], gp_next_free_4k_page);
#endif
	}

	for (i = 0; i < PMM_COUNT; i++)
	{
		pmm_free_page(array[i]);
#if (DEBUG_PMM_MAP_UNMAP)
		printf("free(%p) (next = %p)\n", array[i], gp_next_free_4k_page);
#endif
	}

/*
	// this code maps the VGA screen to the 2G virtual mark and blits crap into it.
	{
		void *phys = (void*)0xb8000;
		uint16 *virt = (void*)0x80000000;
		int count = 16;
		int i;
		vmm_map_pages(virt, phys, count, PTE_KDATA | VMM_PHYS_REAL);
		for (i = 0; i < 80*25; virt[i] = 0x9200 | (i + ' '), i++);
		vmm_unmap_pages(virt, count);
	}
*/
}

// Find a page table in our kernel memory area that has some free virtual addresses.
// Claim these entries for future use a temp virtual pages (used when I need to qucikly
// map a physical page).
static void	init_temp_vpages(void)
{
	int	i;
	uint32	addr = (uint32)&_kernel_temp_vpages_start;

	for (i = 0; i < VMM_TEMP_PAGES; i++, addr += PAGE_SIZE)
	{
		temp_vpages[i] = (void*)addr;
	}
}


// Warning: This function assumes the linker order defined in 'kernel-elf.lds'.
// If the order in that file changes, you must make adjustments here.
void	vmm_init(const struct phys_multiboot_info *mbi)
{
	init_temp_vpages();
	pmm_test();
}

// Called after MBI is copied out of BIOS ram.  Claims all BIOS ram (zero to just under the VGA buffers)
// as available physical memory.  Also claims pages used by .setup.
void	vmm_init_cleanup(void)
{
	uint32		phys_addr;

	for (phys_addr = 0x00000000; phys_addr < 0x000a0000; phys_addr += PAGE_SIZE)
	{
		pmm_free_page((void*)phys_addr);
	}

	for (phys_addr = (uint32)&_setup_start; phys_addr < (uint32)&_setup_end; phys_addr += PAGE_SIZE)
	{
		pmm_free_page((void*)phys_addr);
	}

	printf("vmm: Currently unused physical memory: %d K\n", gp_total_free_4k_pages * 4);
}

/*
static const char *e820_type[5] =
{
	"0",
	"availabe",
	"reserved",
	"acpi",
	"acpi-nvs"
};

static const char *get_e820_type_str(unsigned long type)
{
	if (type < (sizeof(e820_type) / sizeof(e820_type[0])))
	{
		return e820_type[type];
	}

	return "unknown";
}

void	pm_dump_memmap(void)
{
	struct memory_map	*mm = NULL;
	int			i = 0;

	// Dump the memory map.
	for (i = 0, mm = g_e820_memmap; i < g_e820_count; mm++, i++)
	{
		if (mm->type != 1) continue;

		printf("%3d: %08x:%08x   %08x:%08x  %d (%s)\n",
			i, mm->base_addr_high, mm->base_addr_low,
			mm->length_high, mm->length_low, mm->type,
			get_e820_type_str(mm->type));
	}
}
*/
