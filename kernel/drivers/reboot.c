/*	kernel/reboot.c	*/

#include "kernel/kernel.h"

void	hard_reboot(void)
{
	int i = 0;

	con_settextcolor(15, 4);
	printf("hard-reboot!\n");

	if (g_vmware_detected)
	{
		vmware_shutdown();	// should not return.
	}

/* The keyboard controller chip has some IO lines that can control other hardware,
   like disabling NMI or tripping the motherboard reset line.  We will reset that way.
*/

	for (i = 0; i < 255; i++)
	{
		__asm__ __volatile__
		(
			"cli\n"
			"wait_8042:\n"
			"inb $0x64, %%al\n"
			"test $0b00000010, %%al\n"
			"jnz wait_8042\n"
			"mov $0xfe, %%al\n"
			"out %%al, $0x64\n"
			"sti\n"		// Just incase we get here.
			: /* output */ 
			: /* input */
			: /* clobber */ "%ax"
		);
	}

	printf("Reboot failed.\n");

// If the above doesn't reboot us, spin.
	for (;;)
	{
		(*(unsigned int*)(&_kernel_console_start))++;
		__asm__ __volatile__ ("hlt");
	}
}
