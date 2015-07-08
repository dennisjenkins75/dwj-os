/*	kernel/intr.c

	C code handler for all processor interrupts and exceptions.
*/

#include "kernel/kernel.h"

// These are all defined in "kernel/i386.S":
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

struct intr_entry
{
	int	intr;
	void	(*entry_point)(void);
};

static struct intr_entry intr_entry[] =
{
	{ 0, isr0 },
	{ 1, isr1 },
	{ 2, isr2 },
	{ 3, isr3 },
	{ 4, isr4 },
	{ 5, isr5 },
	{ 6, isr6 },
	{ 7, isr7 },
	{ 8, isr8 },
	{ 9, isr9 },
	{ 10, isr10 },
	{ 11, isr11 },
	{ 12, isr12 },
	{ 13, isr13 },
	{ 14, isr14 },
	{ 15, isr15 },
	{ 16, isr16 },
	{ 17, isr17 },
	{ 18, isr18 },
	{ 19, isr19 },
	{ 20, isr20 },
	{ 21, isr21 },
	{ 22, isr22 },
	{ 23, isr23 },
	{ 24, isr24 },
	{ 25, isr25 },
	{ 26, isr26 },
	{ 27, isr27 },
	{ 28, isr28 },
	{ 29, isr29 },
	{ 30, isr30 },
	{ 31, isr31 },

	{ 32, irq0 },
	{ 33, irq1 },
	{ 34, irq2 },
	{ 35, irq3 },
	{ 36, irq4 },
	{ 37, irq5 },
	{ 38, irq6 },
	{ 39, irq7 },
	{ 40, irq8 },
	{ 41, irq9 },
	{ 42, irq10 },
	{ 43, irq11 },
	{ 44, irq12 },
	{ 45, irq13 },
	{ 46, irq14 },
	{ 47, irq15 },

	{ -1, NULL }
};

typedef void (*irq_handler_type)(struct regs *r);

static irq_handler_type irq_handlers[16];

static uint64 interrupt_counter[256];

void	irq_set_handler(int irq, void (*handler)(struct regs *r))
{
	ASSERT(irq >= 0);
	ASSERT(irq < 16);

	irq_handlers[irq] = handler;
}

//  Access flags = 0x8e (entry present, ring-0)
void	intr_install(void)
{
	static struct intr_entry *intr;

// FIXME: Are these necessary?  These globals are in BSS and should be zeroed out
// by the boot loader.
	memset(irq_handlers, 0, sizeof(irq_handlers));
	memset(interrupt_counter, 0, sizeof(interrupt_counter));

	for (intr = intr_entry; intr->intr != -1; intr++)
	{
		idt_set_gate(intr->intr, (uint32)intr->entry_point, GDT_KCODE, 0x8e);
	}

// Remap IRQs 0-15 to interrupts 32 to 47.
	outportb(0x20, 0x11);
	outportb(0xa0, 0x11);
	outportb(0x21, 0x20);
	outportb(0xa1, 0x28);
	outportb(0x21, 0x04);
	outportb(0xa1, 0x02);
	outportb(0x21, 0x01);
	outportb(0xa1, 0x01);
	outportb(0x21, 0x0);
	outportb(0xa1, 0x0);
}


/* This is a simple string array. It contains the message that
*  corresponds to each and every exception. We get the correct
*  message by accessing like:
*  exception_message[interrupt_number] */
char *exception_messages[] =
{
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

void	intr_panic(struct regs *r, const char *name) __attribute__ ((__noreturn__));

void	intr_panic(struct regs *r, const char *name)
{
	unsigned int cr2 = get_cr2();
	unsigned int cr3 = get_cr3();

	uint8 old_attr = con_settextcolor(15, 4);
	printf("cs:%04x ds:%04x EXCPT: %d (%s)\n", r->cs, r->ds, r->int_no, name);
	printf("r2:%08x ax:%08x bx:%08x cx:%08x dx:%08x fl:%08x\n", cr2, r->eax, r->ebx, r->ecx, r->edx, r->eflags);
	printf("r3:%08x ip:%08x sp:%08x bp:%08x si:%08x di:%08x\n", cr3, r->eip, r->esp, r->ebp, r->esi, r->edi);
	if (r->cs != GDT_KCODE)	// FIXME: This is not the correct test.  Test bits in eflags??
	{
		printf("user_ss: %08x   user_esp: %08x\n", r->user_mode_ss, r->user_mode_esp);
	}
	con_set_attr(old_attr);

	PANIC1("Unhandled exception.\n");
}

/*	All exceptions and interrupts call this function from code in i386.S.
	This function should not be called from C code. */
void	interrupt_handler(struct regs *r)
{
	atomic_add64(interrupt_counter + r->int_no, 1);

// CPU Exceptions.
	if (r->int_no < 32)
	{
		if (r->int_no == 14)	// page fault.
		{
			if (vmm_page_fault(r, (void*)get_cr2()))
			{
				return;
			}
		}

		intr_panic(r, exception_messages[r->int_no]);
		return;
	}

// Handle IRQs.
	if (r->int_no < 48)
	{
		void (*handler)(struct regs *r) = irq_handlers[r->int_no - 32];

		if (handler)
		{
			handler(r);
		}

		if (r->int_no >= 40)
		{
			outportb(0xa0, 0x20);	// Send EOI to slave 8259 chip.
		}

		outportb(0x20, 0x20);		// Send EOI to master 8159 chip.

// Only invoke scheduler AFTER we send EOI, so we do it outside of the timer irq handler.
		if (r->int_no == 32)	// IRQ 0 = Timer = Int 32
		{
			schedule();
		}

		return;
	}

	intr_panic(r, "Unmapped interrupt");
}
