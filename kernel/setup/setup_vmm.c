/*	kernel/setup_vmm.c

	The functions in this source file are in the .setup.text section.
	Until paging is turned on, the functions in this source file cannot
	call any functions in .text, nor can it access .data or .bss (directly)
	as they are not mapped yet.  This function is virtual near the 1M address,
	and the rest of the kernel is virtually high (3G or higher).
*/

#include "kernel/kernel.h"

// Points to the address of the next free physical memory page.
// Initialized from the boot loader memory map.  Used by
// _setup_alloc_pages() to grab physical pages.
static uint8* ATTR_SETUP_DATA next_free_page_addr = NULL;

// We won't update this as we allcoate pages.  It stores the
// first free physical address.
static uint8* ATTR_SETUP_DATA first_free_page_addr = NULL;

// Physical address of kernel page dir and various page tables.
static uint32* ATTR_SETUP_DATA kernel_pdir = NULL;

// We update this after we turn on paging.
extern uint16* ATTR_SETUP_DATA _setup_vga_base;

// For internal use only.
static int	ATTR_SETUP_DATA paging_enabled = 0;


#if (DEBUG_SETUP_PAGING)

static void ATTR_SETUP_TEXT _trace(void)
{
	static int index = 0;
	static uint8 ATTR_SETUP_DATA trace_sym = '0';

	*(_setup_vga_base + index++) = 0x4f00 | (trace_sym++);
}

	#define TRACE() _trace()
#else
	#define TRACE()
#endif


// Clears 'count' pages at 'addr' to all zero.  Assumes 'addr' is page aligned.
static inline void ATTR_SETUP_TEXT clear_pages(void *addr, int count)
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


// For the purposes of ".setup", and memory below "&_kernel_virtual" is
// identity mapped, even if those mappings are not fully created.
static inline const void* ATTR_SETUP_TEXT virt_to_phys(const void *virt)
{
	return ((void*)virt > (void*)&_kernel_virtual) ?
		(uint8*)virt - (uint32)&_kernel_virtual :
		virt;
}

static void*	ATTR_SETUP_TEXT _setup_alloc_pages(int count)
{
	void	*ret;

	if (((uint32)next_free_page_addr & PAGE_MASK) != (uint32)next_free_page_addr)
	{
		_setup_log("PANIC: next_free_page_addr is not page-aligned!  %p\n", next_free_page_addr);
		Halt();
	}

	if ((uint32)next_free_page_addr < 0x100000)
	{
		_setup_log("PANIC: next_free_page_addr < 1M!  %p\n", next_free_page_addr);
		Halt();
	}

// FIXME: Need to add code to make sure that we are not out of physical memory.

	ret = (void*)next_free_page_addr;
	next_free_page_addr += PAGE_SIZE * count;

	clear_pages(ret, count);

	return ret;
}


/****************************************************************************

	_setup_map_pages()

	Modifies the future kernel page tables with the given mapping request.
	Will dynamically allocate addtional page tables as necessary.
	Will panic if a virtual page is already mapped.
	Will panic if called when paging is already enabled.

	Note: 'virtual' and 'physical' are page aligned addresses.
*/

static void	ATTR_SETUP_TEXT _setup_map_pages(uint32 virtual, uint32 physical, uint32 count, uint32 flags)
{
	uint32	pde_slot = 0;
	uint32	pte_slot = 0;
	uint32 *pte_table = 0;

#if (DEBUG_SETUP_PAGING)
	_setup_log("map_pages: virt:%p, phys:%p, count:%d, flags:%p\n", virtual, physical, count, flags);
#endif

	if (paging_enabled)
	{
// This function assumes that it is executing under the GRUB flat memory mapping
// (flat segments, paging turned off).  'kernel_pdir' is a physical address.
		_setup_log("setup: Illegal call to _setup_map_pages(%p,%p,%u,%p)\n", 
			(void*)virtual, (void*)physical, count, (void*)flags);
		Halt();
	}

	if (!kernel_pdir)
	{
		_setup_log("kernel_pdir == NULL inside _setup_map_pages().\n");
		Halt();
	}

	if (flags & ~(PTE_ALL_FLAGS))
	{
		_setup_log("illegal flags: %p\n", (void*)flags);
		Halt();
	}

	if (!IS_PAGE_ALIGNED(virtual))
	{
		_setup_log("virtual address, %p, is not page aligned.\n", (void*)virtual);
		Halt();
	}

	if (!IS_PAGE_ALIGNED(physical))
	{
		_setup_log("physical address, %p, is not page aligned.\n", (void*)physical);
		Halt();
	}

	while (count)
	{
		pde_slot = ADDR_TO_PDE_SLOT(virtual);
		pte_slot = ADDR_TO_PTE_SLOT(virtual);

		if (!kernel_pdir[pde_slot])
		{
			uint32 ptbl = (uint32)_setup_alloc_pages(1);
			kernel_pdir[pde_slot] = ptbl | PTE_KDATA;
#if (DEBUG_SETUP_PAGING)
			_setup_log("pde_slot %p allocated at %p\n", pde_slot, ptbl);
#endif
		}

		pte_table = (uint32*)(kernel_pdir[pde_slot] & PTE_MEM_MASK);

		if (pte_table[pte_slot])
		{
			_setup_log("page %p already mapped to %p (physical %p)!\n",
				virtual, pte_table[pte_slot], pte_table[pte_slot] & ~PTE_MEM_MASK);
			Halt();
		}

		pte_table[pte_slot] = physical | flags;

		count--;
		virtual += PAGE_SIZE;
		physical += PAGE_SIZE;
	}
}

#define SPINNER_SIZE 4
static const char ATTR_SETUP_DATA *spinner_str = "-/|\\";

// Low-level function to add pages to our linked list of free pages.
static void	ATTR_SETUP_TEXT	_setup_add_pages(uint32 base, uint32 pages)
{
// We'll hijack the very last 4K page of real-mode memory.   We do this because we know
// that it is already mapped, so its page table entries won't need to be allocated.
	pmm_list_hdr *node = (pmm_list_hdr *)&_kernel_vmm_setup;
	uint32	pde_slot = ADDR_TO_PDE_SLOT((uint32)node);
	uint32	pte_slot = ADDR_TO_PTE_SLOT((uint32)node);
	uint32	*ptbl_phys = NULL;
	uint32	*ptbl_virt = NULL;
	uint32	*clr_ptr = NULL;
	uint32	spinner = 0;

#if (DEBUG_SETUP_PAGING)
	_setup_log("_setup_add_pages(%d, %p, %d)\n", e820_index, base, pages);
#endif

	if (!gp_kernel_page_dir)
	{
		_setup_log("PANIC: _setup_add_pages internal error #0.\n");
		Halt();
	}

	if (!gp_kernel_page_dir[pde_slot])
	{
		_setup_log("PANIC: _setup_add_pages internal error #1 (%x, %x).\n", pde_slot, pte_slot);
		Halt();
	}

	ptbl_phys = (uint32*)(gp_kernel_page_dir[pde_slot] & PTE_MEM_MASK);
	ptbl_virt = (uint32*)((uint32)&_kernel_ptbl_start + (uint32)(pde_slot << 12));

	if ((base & PAGE_MASK) != base)
	{
		_setup_log("PANIC: %p is not page-aligned!\n", base);
		Halt();
	}

	_setup_log("Scanning memory at %p, %d  ", base, pages);

	for (; pages; base += PAGE_SIZE, pages--)
	{
		if (pages % 4096 == 0) _setup_putchar('.');	// 1 per 16 meg.

		*_setup_calc_vram_cursor() = (uint16)(0x0700 | spinner_str[spinner++ % SPINNER_SIZE]);

// If the page is in the range of what we already have loaded or mapped, skip it.
		if (base < (uint32)next_free_page_addr)
		{
#if (DEBUG_SETUP_PAGING)
//			_setup_log("skipping %p\n", base);
#endif
			continue;
		}

// Map this page at our special virtual address.
		ptbl_virt[pte_slot] = base | PTE_KDATA;

// Flush the page tables (this kills performance??)
		InvalidatePage(node);

		// zero fill memory page.
		for (clr_ptr = (uint32*)node; clr_ptr < (uint32*)((uint8*)node + PAGE_SIZE); *(clr_ptr++) = 0);

		node->next = gp_next_free_4k_page;
		node->size = PAGE_SIZE;

		gp_next_free_4k_page = (void*)base;
		gp_total_free_4k_pages++;
	}

	_setup_log("\n");
}

static void	ATTR_SETUP_TEXT	_setup_build_free_page_list (const struct phys_multiboot_info *mbi)
{
	const struct e820_memory_map	*e820 = (const struct e820_memory_map*) mbi->mmap_addr;
	int                     e820_count = mbi->mmap_length / sizeof(struct e820_memory_map);
	int			i = 0;

#if (DEBUG_SETUP_PAGING)
	_setup_log("inside: _setup_build_free_page_list(mbi = %p)\n", mbi);
#endif

	gp_next_free_4k_page = next_free_page_addr;

	for (i = 0; i < e820_count; i++, e820++)
	{
		uint64 base_64 = *(uint64*)&(e820->base_addr_low);      // HACK ALERT!
		uint64 len_64 = *(uint64*)&(e820->length_low);          // HACK ALERT!

		if (e820->type != E820_AVAIL) continue;

// base and len should be page aligned...
		if (base_64 & (PAGE_SIZE - 1))
		{
			// Round length down to next page and base up.
			len_64 -= (base_64 & (PAGE_SIZE - 1));
			base_64 = (base_64 & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
		}

// If length is a partial page, truncate to end block at previous page.
		if (len_64 & (PAGE_SIZE -1))
		{
			len_64 &= ~(PAGE_SIZE - 1);
		}

// We can't handle any memory above 4G yet.
		if (base_64 >= (1LL << 32))
		{
			_setup_log("Warning: memory above 4G not used.\n");
			continue;
		}

// If we start below 1M, and don't extend above it, skip it.  We won't allocate physical pages
// from < 1M.
		if (base_64 < 0x100000)
		{
			if (base_64 + len_64 > 0x100000)
			{
				_setup_log("Warning: memory spans 1M boundry.  Skipping.\n");
			}
			continue;
		}

// If base starts below 4G but spans across it, truncate it.
		if ((base_64 + len_64) >= (1LL << 32))
		{
			_setup_log("Warning: memory above 4G not used.\n");
			len_64 -= (base_64 & ~((1LL << 32) - 1));
		}

// _setup_log("phys_mem: %08x to %08x (%d K)\n", (uint32)base_64, (uint32)(base_64 + len_64), (uint32)(len_64 / 1024));

// Break memory chunk on 4M boundaries, and submit each chunk seperately.
#if 0
		_setup_log("e820(%d): start = %p, pages = %p\n", i, (uint32)base_64, (uint32)len_64 / PAGE_SIZE);

		{
			uint32 start = (uint32)base_64;
			uint32 pages_left = (uint32)(len_64 / PAGE_SIZE);
			uint32 pages_this = 0;
			uint32 end = 0;

			while (pages_left)
			{
// Round "start" up to next 4M bounary.
				end = (start + (1 << 22)) & ~((1 << 22) - 1);

// How many pages can we do right now so that "start + pages_this" is on a 4M boundry?
				pages_this = min((end - start) >> 12, pages_left);

_setup_log("start = %p, end = %p, pages = %p, left = %p\n", start, end, pages_this, pages_left);
//Halt();
				_setup_add_pages(start, pages_this);
				start += pages_this * PAGE_SIZE;
				pages_left -= pages_this;
			}
		}
#else
		_setup_add_pages((uint32)base_64, (uint32)len_64 / PAGE_SIZE);
#endif
	}

#if (DEBUG_SETUP_PAGING)
	_setup_log("gp_next_free_4k_page = %p\n", gp_next_free_4k_page);
	_setup_log("gp_total_free_4k_pages = %d (%d K, %d M)\n", gp_total_free_4k_pages, gp_total_free_4k_pages * 4, gp_total_free_4k_pages /256);
#endif
}


/* Used to pre-allocate physical pages into the page directory for
   page tables of virtual addresses that we won't explicitly map just yet.
   Mostly these are the "__kernel_" pointers defined in the linker map
   file ("kernel-elf.lds"). */
static void	ATTR_SETUP_TEXT prealloc_page_table(const void *virtual)
{
	int	pde_slot = ADDR_TO_PDE_SLOT(virtual);
	void	*page = NULL;

	if (!IS_PDIR_ALIGNED(virtual))
	{
		_setup_log("PANIC: alloc_page_table(%p) failed.  Not alligned to 4M page.\n", virtual);
		Halt();
	}

	if (!kernel_pdir)
	{
		_setup_log("PANIC: alloc_page_table(%p) internal error.  kernel_pdir == NULL.\n", virtual);
		Halt();
	}

	if (!kernel_pdir[pde_slot])
	{
		page = _setup_alloc_pages(1);
		kernel_pdir[pde_slot] = (uint32)page | PTE_KDATA;
#if (DEBUG_SETUP_PAGING)
		_setup_log("AFTER:  prealloc(%p) = [%x] %p\n", virtual, pde_slot, kernel_pdir[pde_slot]);
#endif
	}
	else
	{
#if (DEBUG_SETUP_PAGING)
		_setup_log("BEFORE: prealloc(%p) = [%x] %p\n", virtual, pde_slot, kernel_pdir[pde_slot]);
#endif
	}
}


/*******************************************************************************

	We need to locate the first free physical memory page.  The boot loader
	is supposed to tell us where it loaded our modules.  Grub will load items
	physically in this order, starting at the 1M address:

	Our kernel (segment order set by linker):
		.setup
		.text
		.data
		.bss

	Kernel's ELF header (page aligned)

	Modules listed in the Grub config (page aligned).
		module[0]
		module[1]
		module[n-1].

	So, if we don't have any modules, we need to find the size of the ELF header.
*/

#if (DEBUG_SETUP_PAGING)
static const char ATTR_SETUP_RODATA str_dump_sec[] = "%s\t = %p to %p, (phys: %p to %p)\n";
static const char ATTR_SETUP_RODATA str_setup[] = ".setup";
static const char ATTR_SETUP_RODATA str_text[] = ".text";
static const char ATTR_SETUP_RODATA str_data[] = ".data";
static const char ATTR_SETUP_RODATA str_bss[] = ".bss";
static const char ATTR_SETUP_RODATA str_mods[] = "modules";
#endif

// This function is called from our assmebly language startup code.
// The "setup console driver" is already initialized, so we are ready to print stuff
// via "_setup_log".
void	ATTR_SETUP_TEXT _setup_init_paging(const struct phys_multiboot_info *mbi)
{
	int		physical = 0;
	int		virtual = 0;
	int		pages = 0;
	int		pde_slot = 0;
	uint32		modules_start = 0;
	uint32		modules_end = 0;
	uint32		module_pages = 0;
	struct phys_mb_module	*mod = NULL;

// Compute physical address of first available physical memory page.
	if (mbi->mods_count)
	{
		mod = (struct phys_mb_module*)(mbi->mods_addr);
		modules_start = (uint32)(mod->mod_start);
		mod += (mbi->mods_count - 1);
		modules_end = (uint32)(PAGE_AFTER(mod->mod_end) << PAGE_BITS);
		module_pages = (mod->mod_end - modules_start + PAGE_SIZE - 1) / PAGE_SIZE;

		next_free_page_addr = (uint8*)modules_end;
		first_free_page_addr = next_free_page_addr;
	}
	else
	{
		mod = NULL;
		modules_start = modules_end = module_pages = 0;

		next_free_page_addr = (uint8*)(PAGE_AFTER(virt_to_phys(&_bss_end)) << PAGE_BITS);
		first_free_page_addr = next_free_page_addr;
	}

#if (DEBUG_SETUP_PAGING)
	_setup_log(str_dump_sec, str_setup, &_setup_start, &_setup_end,
		virt_to_phys(&_setup_start), virt_to_phys(&_setup_end));

	_setup_log(str_dump_sec, str_text, &_text_start, &_text_end,
		virt_to_phys(&_text_start), virt_to_phys(&_text_end));

	_setup_log(str_dump_sec, str_data, &_data_start, &_data_end,
		virt_to_phys(&_data_start), virt_to_phys(&_data_end));

	_setup_log(str_dump_sec, str_bss, &_bss_start, &_bss_end,
		virt_to_phys(&_bss_start), virt_to_phys(&_bss_end));

	_setup_log(str_dump_sec, str_mods, modules_start, modules_end,
		virt_to_phys((void*)modules_start), virt_to_phys((void*)modules_end));
#endif

	kernel_pdir = (uint32*)_setup_alloc_pages(1);
#if (DEBUG_SETUP_PAGING)
	_setup_log("kernel_page_dir = %p\n", kernel_pdir);
#endif

// Map the page directory to itself as if it were also a page table.
// This makes all page dir entries appear as page tables at 0xfffff000.
// See code at bottom of this function that sets "gp_kernel_page_dir".
	pde_slot = ADDR_TO_PDE_SLOT((uint32)&_kernel_ptbl_start);
	if (pde_slot != 1023)
	{
		_setup_log("PANIC: kernel not configured for page dir virt addr: %p\n", &_kernel_ptbl_start);
		Halt();
	}
	kernel_pdir[pde_slot] = (uint32)kernel_pdir | PTE_KDATA;

// Need a blank page table for a few things that won't get mapped until later.
	prealloc_page_table(&_kernel_console_start);
	prealloc_page_table(&_kernel_stack_start);
	prealloc_page_table(&_kernel_temp_vpages_start);
	prealloc_page_table(&_kernel_vmm_setup);

// Map the .setup section (make it read/write, as it contains code + data).
// Need to retain it mapped low.  We will still be executing out of it after loading
// the new page tables.
	pages = PAGE_AFTER(&_setup_end) - PAGE_OF(&_setup_start);
	virtual = (uint32)&_setup_start;
	physical = virtual;
	_setup_map_pages(virtual, physical, pages, PTE_KDATA);

// Map the kernel CODE section (.text, .rodata)
	pages = PAGE_AFTER(&_text_end) - PAGE_OF(&_text_start);
	virtual = (uint32)&_text_start;
	physical = (uint32)&_text_start - (uint32)&_kernel_virtual;
	_setup_map_pages(virtual, physical, pages, PTE_KCODE);

// Map the kernel DATA section (.data)
	pages = PAGE_AFTER(&_data_end) - PAGE_OF(&_data_start);
	virtual = (uint32)&_data_start;
	physical = (uint32)&_data_start - (uint32)&_kernel_virtual;
	_setup_map_pages(virtual, physical, pages, PTE_KDATA);

// Map the kernel BSS section (.bss, .common)
	pages = PAGE_AFTER(&_bss_end) - PAGE_OF(&_bss_start);
	virtual = (uint32)&_bss_start;
	physical = (uint32)&_bss_start - (uint32)&_kernel_virtual;
	_setup_map_pages(virtual, physical, pages, PTE_KDATA);

// Map the future kernel stack.  We need to steal some physical pages for this.
	pages = (uint32)&_kernel_stack_size / PAGE_SIZE;
	virtual = (uint32)&_kernel_stack_start;
	physical = (uint32)_setup_alloc_pages(pages);
	_setup_map_pages(virtual, physical, pages, PTE_KDATA);

// Map future VGA screen (128K) (DOS: a0000...bffff)
	pages = 65536 * 2 / PAGE_SIZE;
	virtual = (uint32)&_kernel_console_start;
	physical = 0xa0000;
	_setup_map_pages(virtual, physical, pages, PTE_KDATA);

// Map real-mode ram 1:1, so that we can access the multi-boot-info structure.
// NOTE: This memory will be unmapped shortly after the .setup ends.
// Except: we will skip the first 1 page of memory, so that if we dereference
// a NULL pointer during .setup, we will crash (and find the bug earlier).
	pages = BIOS_PAGE_COUNT - 1;	// skip first page.
	virtual = PAGE_SIZE;
	physical = PAGE_SIZE;
	_setup_map_pages(virtual, physical, pages, PTE_KDATA);

// we should not need this.  Page tables should be self-mapped via cr3[1023] = &pde[0];
// Map any newly allocated pages (page tables mostly).
//	pages = PAGE_AFTER(next_free_page_addr) - PAGE_OF(first_free_page_addr);
//	virtual = (uint32)first_free_page_addr;
//	physical = virtual;
//	_setup_map_pages(virtual, physical, pages, PTE_KDATA);

// Turn on paging.
	_setup_log("setup: Enabling paging, setting %%cr3 = %p\n", kernel_pdir);

	__asm__ __volatile__
	(
		"movl	%0, %%eax\n"
		"movl	%%eax, %%cr3\n"
		"movl	%%cr0, %%eax\n"
		"orl	%1, %%eax\n"
		"movl	%%eax, %%cr0\n"
		: /* outputs */
		: /* inputs */		"m" (kernel_pdir), "i" (CR0_PG_MASK | CR0_WP_MASK)
		: /* clobbers */	"%eax"
	);

// Old BIOS memory range is no longer valid.  Must use new one.
	_setup_vga_base = (uint16*)((uint8*)&_kernel_console_start + 0x18000);
	paging_enabled = 1;

	TRACE();	// test BIOS mapping.

// Verify that paging works, that we can read from each page that we just mapped.
// Not sure why, but _setup_log() will now triple-fault (via unhandled page fault)
// if we access any strings via ".data - &_kernel_virtual" (which maps to .setup.data).
// See the hack in _setup_log().
#if (DEBUG_SETUP_PAGING && 0)
	{
		static const char str[] ATTR_SETUP_RODATA = "Testing page tables: %p\n";
		char	dummy;
		int	i;

		for (i = (uint32)&_text_start; i < (uint32)&_bss_end; i += PAGE_SIZE)
		{
			_setup_log(str, i);
			dummy = *(char*)i;	// will either work or crash!
			TRACE();
		}

		for (i = (uint32)&_setup_start; i < (uint32)&_setup_end; i += PAGE_SIZE)
		{
			_setup_log(str, i);
			dummy = *(char*)i;	// will either work or crash!
			TRACE();
		}

		for (i = PAGE_SIZE; i < PAGE_SIZE * BIOS_PAGE_COUNT; i += PAGE_SIZE)
		{
			_setup_log(str, i);
			dummy = *(char*)i;	// will either work or crash!
			TRACE();
		}

// Testing: Force a page fault (exception handlers not installed yet, so this will triple-fault).
//		_setup_log(str, 0x80000000);
//		dummy = *(char*)0x80000000;
//		_setup_log(str, 0x00000000);
//		dummy = *(char*)0x00000000;
	}
#endif // DEBUG_SETUP_PAGING

// Since the '.setup' section is going away shortly, we need to store some
// important data in the kernel's '.data' section.  We can only do this now, after
// paging is turned on.

// gp_kernel_page_dir is the new VIRTUAL address of the kernel's page directory.
// "kernel_pdir" (private to this source file) is ".setup"'s virtual and physical
// address of the same (same as the value in %cr3".
	gp_kernel_page_dir = (uint32*)((uint32)&_kernel_ptbl_start + 0x003ff000);

	if (gp_kernel_page_dir != (uint32*)0xfffff000)
	{
		// If we get here, then the linker file "kernel-elf.lds" has been modifed
		// out of sync with this C code.
		_setup_log("invalid gp_kernel_page_dir %p (line %d, setup_vmm.c)\n", gp_kernel_page_dir, __LINE__);
		Halt();
	}

#if (DEBUG_SETUP_PAGING && 0)
	{
		// Dump the newly mapped page directory entries.
		int	pde;

		_setup_log("Page directory entries initialized:\n");
		for (pde = 0; pde < 1024; pde++)
		{
			if (gp_kernel_page_dir[pde] & PTE_PRESENT)
			{
				_setup_log("\tpde: [%p] = %p\n", pde << (PTE_BITS + PAGE_BITS), gp_kernel_page_dir[pde] & PDIR_MASK);
			}
		}
	}
#endif

// Note that the linker places these symbols at their virtual address, so we
// must adjust for their physical address.

	g_code_start = (uint32)&_text_start - (uint32)&_kernel_virtual;
	g_code_pages = PAGE_AFTER(&_text_end) - PAGE_OF(&_text_start);

	g_data_start = (uint32)&_data_start - (uint32)&_kernel_virtual;
	g_data_pages = PAGE_AFTER(&_bss_end) - PAGE_OF(&_data_start);

	g_modules_start = modules_start;
	g_module_pages = module_pages;

// Using the Multi-boot memory map, add all of the available physical memory pages
// into a linked list.  This involves temporarily mapping and unmapping all of the pages.
	{
		uint64 start = read_tsc();

	_setup_build_free_page_list(mbi);

		uint64 diff = read_tsc() - start;
		// setup printf() can't do 64 bit args.
		_setup_log("setup time = %p,%p\n", (uint32)(diff >> 32), (uint32)diff);
	}

	TRACE();
}
