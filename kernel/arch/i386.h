/*	kernel/i386.h
*/


#define GDT_ENTRIES	6
#define	GDT_NULL	0x00
#define GDT_KCODE	0x08
#define GDT_KDATA	0x10
#define GDT_UCODE	(0x18 | 0x03)
#define GDT_UDATA	(0x20 | 0x03)
#define GDT_TSS		0x28

// This defines what the stack looks like after an ISR was running.
struct regs
{
	unsigned long int gs, fs, es, ds;
	unsigned long int edi, esi, ebp, esp, ebx, edx, ecx, eax;
	unsigned long int int_no, err_code;
	unsigned long int eip;
	unsigned long int cs;
	unsigned long int eflags;
	unsigned long int user_mode_esp;	// only on ring 1,2,3 -> ring 0
	unsigned long int user_mode_ss;		// only on ring 1,2,3 -> ring 0
} __attribute__((packed)) ;

// Any instances of "tss_t' MUST be packed.  Ex:
// struct tss_t __attribute__((packed)) my_tss;
struct tss_t
{
	uint16	backlink, __blh;
	uint32	esp0;
	uint16	ss0, __ss0h;
	uint32	esp1;
	uint16	ss1, __ss1h;
	uint32	esp2;
	uint16	ss2, __ss2h;
	uint32	cr3;
	uint32	eip;
	uint32	eflags;
	uint32	eax;
	uint32	ecx;
	uint32	edx;
	uint32	ebx;
	uint32	esp;
	uint32	ebp;
	uint32	esi;
	uint32	edi;
	uint16	es, __esh;
	uint16	cs, __csh;
	uint16	ss, __ssh;
	uint16	ds, __dsh;
	uint16	fs, __fsh;
	uint16	gs, __gsh;
	uint16	ldt, __ldth;
	uint16	trace;
	uint16	io_bitmap_offset;

// Extended parts of the TSS.
	uint32	io_bitmap[65536 / sizeof(uint32)];
} __attribute__((packed));

struct gdt_entry_t
{
	uint16	limit_low;
	uint16	base_low;
	uint8	base_middle;
	uint8	access;
	uint8	granularity;
	uint8	base_high;
} __attribute__((packed));

struct gdt_ptr_t
{
	uint16	limit;
	uint32	base;
} __attribute__((packed));

static inline uint32 get_cr3(void)
{
	register uint32 r;
	__asm__ __volatile__ ( "movl %%cr3, %0" : "=r"(r) );
	return r;
}

static inline uint32 get_cr2(void)
{
	register uint32 r;
	__asm__ __volatile__ ( "movl %%cr2, %0" : "=r"(r) );
	return r;
}

static inline void set_cr3(uint32 value)
{
	__asm__ __volatile__ ( "movl %0, %%cr3" : : "a"(value) );
}

// Returns non-zero if interrupts are enabled, 0 if disabled.
static inline int are_irqs_enabled(void)
{
	register char ret;

	__asm__ __volatile__
	(
		"pushf			\n"
		"popl	%%eax		\n"
		"andl	$512, %%eax	\n"  // bit #9
		"setnz	%0		\n"
		: "=q"(ret)
	);

	return (int)ret;
}

static inline void undef_opcode(void)
{
	__asm__ __volatile__ ( "ud2" );
}

static inline uint64 read_tsc(void)
{
	register uint64 tsc __asm__ ("eax");
	__asm__ __volatile__ ("rdtsc" ::: "%eax", "%edx");	// places result in EDX:EAX
	return tsc;
}

#define DebugBreak()  __asm__ __volatile__ ("int $3")
#define Halt() __asm__ __volatile__ ("cli;hlt")
#define Nop() __asm__ __volatile__ ("nop;nop;nop;nop")
#define InvalidatePage(x) __asm__ __volatile__("invlpg %0"::"m" (*(char *)x));



extern void  switch_stacks(uint32 new_esp, uint32 *old_esp);

