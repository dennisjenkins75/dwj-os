/*	init_paging(mbi):

	Calculates the addresses of the kernel's sections and the modules
	loaded by grub.  Sets up the initial paging scheme to properly
	enable virtual memory.
*/

/*

void	init_paging(multiboot_info_t *mbi)
{
#if 0
        // Pointers to the page directory and the page table
        void	*kernelpagedirPtr = 0;
	void	*p_bios = 0;
	void	*p_kernel = 0;
        int	k = 0;
	int	i = 0;
	multiboot_info_t *mbi_virt = (multiboot_info_t*)PHYS_TO_VIRT(mbi);
	mb_module_t	*mod;

// On entry, "mbi" is a physical address.

// ".setup" and ".text" sections are physically contiguous, with no "alignment"
// when ".setup" ends and ".text" begins.  They are both executable and should be read-only.

// Also note that the linker places these symbols at their virtual address, so we
// must adjust for their physical address.

	g_code_start = VIRT_TO_PHYS(&_setup_text_start);
	g_code_pages = (VIRT_TO_PHYS(&_text_end) - g_code_start + PAGE_SIZE - 1) / PAGE_SIZE;

	g_data_start = VIRT_TO_PHYS(&_data_start);
	g_data_pages = (VIRT_TO_PHYS(&_bss_end) - g_data_start + PAGE_SIZE - 1) / PAGE_SIZE;

// Compute the start and size of all modules.
// FIXME: We make an assumption that GRUB loads these sequentially, so all that we
// really need is the last one.  Not sure if this is safe.

	g_mods_start = g_mods_pages = 0;

// 'mod' is array of 'mb_module_t', the list of modules.
	if (mbi_virt->mods_count)
	{
		mod = (mb_module_t*)PHYS_TO_VIRT((mb_module_t*)(mbi_virt->mods_addr));
		g_mods_start = mod->mod_start;
		mod += (mbi_virt->mods_count - 1);
		g_mods_pages = (mod->mod_end - g_mods_start + PAGE_SIZE - 1) / PAGE_SIZE;
	}

	printf("kernel CODE: %08x, %d pages\n", g_code_start, g_code_pages);
	printf("kernel DATA: %08x, %d pages\n", g_data_start, g_data_pages);
	printf("modules:     %08x, %d pages\n", g_mods_start, g_mods_pages);

#endif
}
*/

