/*	kernel/multiboot.c */

#include "kernel.h"

void	dump_mboot_info(const struct phys_multiboot_info *mbi)
{
	const char *cmdline = (mbi->flags & 2) && mbi->cmdline ? (const char *)mbi->cmdline : "";
	struct phys_mb_module	*mbm = NULL;
	int  i;

	printf("mbi (raw):       %08x\n", mbi);
	printf("mbi.flags:       %08x\n", mbi->flags);
	printf("mbi.mem_lower:   %08x  (%d Kb)\n", mbi->mem_lower, mbi->mem_lower);
	printf("mbi.mem_upper:   %08x  (%d Mb)\n", mbi->mem_upper, mbi->mem_upper / 1024);
	printf("mbi.boot_device: %08x\n", mbi->boot_device);
	printf("mbi.cmdline:     %08x (%s)\n", mbi->cmdline, cmdline);
	printf("mbi.mods_count:  %d\n", mbi->mods_count);
	printf("mbi.mods_addr:   %08x\n", mbi->mods_addr);
	printf("mbi.mmap_length: %08x\n", mbi->mmap_length);
	printf("mbi.mmap_addr:   %08x\n", mbi->mmap_addr);

// Enumerate loaded modules.
	if (mbi->flags & (1 << 3))
	{
		mbm = (struct phys_mb_module*)(mbi->mods_addr);
		for (i = 0; i < mbi->mods_count; i++, mbm++)
		{
			printf("Module %2d: %p, %p, (%p) %s\n", i, mbm->mod_start, mbm->mod_end,
				mbm->string, mbm->string ? (const char*)mbm->string : "");
		}
	}

// We should not have an A.OUT header, but what the heck.
	if (mbi->flags & (1 << 4))
	{
		const struct phys_aout_header *aout_sym = &(mbi->u.aout_sym);

		printf ("aout_symbol_table: tabsize = 0x%0x, strsize = 0x%x, addr = 0x%x\n",
			(unsigned) aout_sym->tabsize,
			(unsigned) aout_sym->strsize,
			(unsigned) aout_sym->addr);
	}

// Enumerate ELF header.
	if (mbi->flags & (1 << 5))
	{
		const struct phys_elf_header *elf_sec = &(mbi->u.elf_sec);

		printf ("elf_sec: num = %u, size = 0x%x, addr = 0x%x, shndx = 0x%x\n",
			(unsigned) elf_sec->num, (unsigned) elf_sec->size,
			(unsigned) elf_sec->addr, (unsigned) elf_sec->shndx);
         }

}

/* Grub puts the original multi-boot header in low memory.  We want to reclaim
   low-memory for other uses.  "relocate_mbi" will do this.  It requires
   the heap, so we can't call it until the heap is ready. */

struct multiboot	*gp_MultiBootInfo = NULL;

void	relocate_mbi(const struct phys_multiboot_info *mbi)
{
	int	i;
	uint32	next_mod_addr = (uint32)&_kernel_mod_map_start;

	gp_MultiBootInfo = (struct multiboot*)kmalloc(sizeof(struct multiboot), 0);

// Basic stuff.
	gp_MultiBootInfo->flags = mbi->flags;
	gp_MultiBootInfo->mem_lower = mbi->mem_lower;
	gp_MultiBootInfo->mem_upper = mbi->mem_upper;
	gp_MultiBootInfo->boot_device = mbi->boot_device;
	gp_MultiBootInfo->cmdline = strdup((const char*)mbi->cmdline);

// Modules.
	gp_MultiBootInfo->mods_count = mbi->mods_count;
	gp_MultiBootInfo->mods_array = (struct mb_module*)kmalloc(mbi->mods_count * sizeof(struct mb_module), 0);

	if (mbi->flags & (1 << 3))
	{
		for (i = 0; i < mbi->mods_count; i++)
		{
			struct mb_module *mod = gp_MultiBootInfo->mods_array + i;
			struct phys_mb_module *src = ((struct phys_mb_module *)mbi->mods_addr) + i;

			mod->start_phys = (void*)(src->mod_start);
			mod->size = src->mod_end - src->mod_start;
			mod->string = strdup((char*)(src->string));
			mod->start_virt = NULL;

			FIXME(); // this logic is not fool proof.  An insanely huge mod would wrap the address space.
			if (next_mod_addr + mod->size < (uint32)&_kernel_mod_map_end)
			{
				int pages = (mod->size + PAGE_SIZE - 1) / PAGE_SIZE;

				mod->start_virt = (void*)next_mod_addr;
				next_mod_addr += pages * PAGE_SIZE;
				vmm_map_pages(mod->start_virt, mod->start_phys, pages, (PTE_RO | PTE_PRESENT | PTE_SUPERVISOR | VMM_PHYS_REAL));

				if (!strncmp(mod->start_virt, "VAST", 4))
				{
					g_pVastMapAddr = mod->start_virt;
				}
			}
		}
	}

// Elf-headers.
	if (mbi->flags & (1 << 5))
	{
		FIXME();
		gp_MultiBootInfo->elf_hdr_count = 0;
		gp_MultiBootInfo->elf_headers = NULL;	// FIXME: Code this some day.
	}

// BIOS memory map.
	gp_MultiBootInfo->e820_count = mbi->mmap_length / sizeof(struct e820_memory_map);
	i = gp_MultiBootInfo->e820_count * sizeof(struct e820_memory_map);
	gp_MultiBootInfo->e820_table = (struct e820_memory_map*)kmalloc(i, 0);
	memcpy(gp_MultiBootInfo->e820_table, mbi->mmap_addr, i);
}
