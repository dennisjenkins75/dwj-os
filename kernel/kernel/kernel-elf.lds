/* 	kernel/link.ld

Example kernel memory map:

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .setup        00000b4c  00100000  00100000  00001000  2**2
                  CONTENTS, ALLOC, LOAD, CODE
  1 .text         000035fb  f0101000  00101000  00002000  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  2 .data         00000120  f0105000  00105000  00006000  2**5
                  CONTENTS, ALLOC, LOAD, DATA
  3 .bss          000069a8  f0106000  00106000  00006120  2**12
                  ALLOC
*/

OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

phys = 0x00100000;	/* 1 Meg */
virt = 0xf0000000;	/* 3.75 Gig */

__kernel_vmm_setup		= 0x80000000;	/* see setup_vmm.c; used as temp ptable. */

__kernel_heap_start		= 0xf1000000;
__kernel_heap_end		= 0xf9000000;	/* 128M heap? */
__kernel_console_start		= 0xff000000;	/* Needs 32K for text console. */
__kernel_stack_start 		= 0xff400000;
__kernel_temp_vpages_start	= 0xff800000;
__kernel_ptbl_start		= 0xffc00000;	/* goes until top of memory. */

__kernel_stack_size		= 4096;

/* We map our grub loaded modules from 0xe0000000 to 0xefffffff. */
__kernel_mod_map_start		= 0xe0000000;
__kernel_mod_map_end		= 0xf0000000;


SECTIONS
{
	__kernel_virtual = virt;

	. = phys;

	.setup ALIGN(4096) :
	{
		*(.setup)
		*(.setup.text)
		*(.setup.rodata)
		*(.setup.data)
		*(.setup.bss)
		__setup_end = ALIGN(4096);
	}

	. += virt;

	.text ALIGN(4096) : AT (ADDR(.text) - virt)
	{
		*(.text)
		*(.rodata*)
		__text_end = ALIGN(4096);
	}

	.data ALIGN (4096) : AT (ADDR(.data) - virt)
	{
		*(.data)
		__data_end = ALIGN(4096);
	}

	.bss ALIGN (4096) : AT (ADDR(.bss) - virt)
	{
		*(.bss*)
		*(COMMON*)
		__bss_end = ALIGN(4096);
	}

	/DISCARD/ :
	{
		*(.comment)
		*(.note.GNU-stack)
	}
}
