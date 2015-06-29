/*	kernel/main.c */

#include "kernel/kernel/kernel.h"

/*	On entry the following will be already initialized:
	1) Paging.
		a) First 1M of RAM will be identity mapped.
		b) Kernel sections .gdt, .setup.* will be identity mapped.
		c) Kernel .test, .data and .bss will be mapped high.
	2) GDT.
		a) Standard descriptors are loaded (null, kcode, kdata).
*/

char	g_kcmdline[256];	// kernel command line,copied from multi-boot thingy.

struct corehelp corehelp;

void	init_corehelp(void)
{
	memset(&corehelp, 0, sizeof(corehelp));

	corehelp.magic1 = CORE_HELP_MAGIC1;
	corehelp.size = sizeof(corehelp);
	corehelp.magic2 = CORE_HELP_MAGIC2;
	corehelp.cr3 = get_cr3();

//	printf("corehelp = %p\n", &corehelp);

	// the remaining fields will get initialized by the various sub-systems.
}

int	kmain (const struct phys_multiboot_info *mbi)
{
	char temp[16];
	int hz = 100;
	int video_mode = VGA_MODE_80x25;
	char *p = NULL;
	int r = 0;
	int do_fs_init = 0;

	init_corehelp();
	outportb(0x3f2, 0);	// shut off the floppy disk motor.
	gdt_install();
	idt_install();
	intr_install();

	strcpy_s(g_kcmdline, sizeof(g_kcmdline), (mbi->flags & 2) ? (const char*)(mbi->cmdline) : "");

	if (NULL != (p = k_getArg(g_kcmdline, temp, sizeof(temp), "vga")))
	{
		if (!strcmp(p, "80x25")) video_mode = VGA_MODE_80x25;
		else if (!strcmp(p, "80x50")) video_mode = VGA_MODE_80x50;
		else if (!strcmp(p, "90x30")) video_mode = VGA_MODE_90x30;
		else if (!strcmp(p, "90x60")) video_mode = VGA_MODE_90x60;
		else video_mode = VGA_MODE_80x25;
	}

	if (NULL != (p = k_getArg(g_kcmdline, temp, sizeof(temp), "hz")))
	{
		hz = atoi(p);

		if ((hz < 1) || (hz > 99999))
		{
			hz = 20;
		}
		printf("HZ = '%d'.\n", hz);
	}

	do_fs_init = NULL != k_getArg(g_kcmdline, temp, sizeof(temp), "fsinit");

	con_init(video_mode);
	test_spinlocks();

	if (!mbi || ((uint32)mbi > 0x9ffff))
	{
		PANIC2("Multiboot Info Pointer (%p) not in low-mem!\n", mbi);
	}

//	dump_mboot_info(mbi);
	vmm_init(mbi);
	heap_init();
	relocate_mbi(mbi);	// Now that we have a heap we can do this.
	vmm_init_cleanup();	// Reclaim BIOS memory, .setup sections.

	obj_init();
	test_snprintf();

	ramfs_init();
	devfs_init();

	if (do_fs_init)
	{
		if (0 > (r = vfs_mount("ramfs", "/", "", "my_gf=pookie")))
		{
			PANIC3("Failed to mount root fs: %d (%s).\n", r, strerror(r));
		}

		if (0 > (r = vfs_mkdir("/dev")))
		{
			PANIC3("mkdir(\"/dev\") failed: %d (%s).\n", r, strerror(r));
		}

		if (0 > (r = vfs_mount("devfs", "/dev", "", "opts_go_here=1")))
		{
			PANIC3("Failed to mount devfs: %d (%s).\n", r, strerror(r));
		}

		Halt();
	}

	scheduler_init();	// creates idle, reaper threads.

	timer_install();
	set_timer_phase(hz);
	keyboard_install();

	printf("enabling interrupts...\n");
	enable();

// From this point on, this thread will NOT run if we have ANY runnable threads.
// So we'll create a thread whose sole purpose is to create the other initial threads.

	task_create(ktask_startup_entry, NULL, "[startup]", RUNNABLE);

	while (1)
	{
		__asm__ __volatile__ ("hlt");
	}

	return 0;
}
