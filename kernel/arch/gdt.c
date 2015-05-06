/*	kernel/gdt.c

	Original code from "Bran's Kernel Development Tutorial" (friesenb@gmail.com)
	http://www.osdever.net/bkerndev/Docs/gdt.htm

*/

#include "kernel/kernel.h"

struct tss_t tss;

static struct gdt_entry_t gdt[GDT_ENTRIES];
static struct gdt_ptr_t gp;

static void	gdt_set_gate(uint16 selector, uint32 base, uint32 limit, uint8 access, uint8 granularity)
{
	struct gdt_entry_t *g = gdt + (selector / 0x08);

	ASSERT((selector / 8) < GDT_ENTRIES);

	g->base_low = (base & 0xFFFF);
	g->base_middle = (base >> 16) & 0xFF;
	g->base_high = (base >> 24) & 0xFF;

	g->limit_low = (limit & 0xFFFF);
	g->granularity = ((limit >> 16) & 0x0F);

	g->granularity |= (granularity & 0xF0);
	g->access = access;
}

void	gdt_install(void)
{
	gp.limit = (sizeof(struct gdt_entry_t) * GDT_ENTRIES) - 1;
	gp.base = (unsigned int)&gdt;

	memset(&tss, 0, sizeof(tss));

	tss.ss0 = GDT_KDATA;
	tss.io_bitmap_offset = offsetof(struct tss_t, io_bitmap);

	gdt_set_gate(GDT_NULL, 0, 0, 0, 0);
	gdt_set_gate(GDT_KCODE, 0, 0xffffffff, 0x9a, 0xcf);
	gdt_set_gate(GDT_KDATA, 0, 0xffffffff, 0x92, 0xcf);
	gdt_set_gate(GDT_UCODE, 0, 0xffffffff, 0xfe, 0xcf);
	gdt_set_gate(GDT_UDATA, 0, 0xffffffff, 0xf2, 0xcf);
	gdt_set_gate(GDT_TSS, (uint32)&tss, sizeof(tss), 0x89, 0xcf);

	__asm__ __volatile__
	(
		"lgdt	_gp\n"
		"mov	%0, %%ax\n"	// %0 = GDT_KDATA
		"mov	%%ax, %%ds\n"
		"mov	%%ax, %%es\n"
		"mov	%%ax, %%fs\n"
		"mov	%%ax, %%gs\n"
		"mov	%%ax, %%ss\n"
		"jmpl	%1, $done\n"	// %1 = GDT_KCODE
		"done: nop\n"
		: /* outputs */
		: /* inputs */ "i"(GDT_KDATA), "i"(GDT_KCODE)
		: /* clobbers */ "%eax"
	);

	__asm__ __volatile__
	(
		"ltr	%%ax\n" :: "a"(GDT_TSS)
	);
}
