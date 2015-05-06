#       kernel/i386.S
#
#  This source module implements the Intel interrupt and exception handlers.
#  When an interrupt occurs, the stack will look like this (see "struct regs")
#  in "i386.h".  The structure's element order matches the stack frame.
#
#	SS 		(only if interrupted task was in ring 1,2,3)
#	ESP		(only if interrupted task was in ring 1,2,3)
#	eflags
#	CS
#	EIP
#	Exception Code (or 0)
#	Absolute intr #
#	EAX
#	ECX
#	EDX
#	EBX
#	ESP
#	EBP
#	ESI
#	EDI
#	DS
#	ES
#	FS
#	GS

.code32

# Create some space between "__text_start" and "_isr_common_stub".
# Purely so that the symbolic stack dumping code can tell the difference
# between the two.
	nop
	nop
	nop
	nop

####################################################
#
#	All interrupts and exceptions will ultimately land here.
#	This code will finish setting up the stack frame and
#	call the C-code handler.

.extern 	_interrupt_handler

.global _isr_common_stub
_isr_common_stub:
	pusha
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	movw	$0x10, %ax		# FIXME: magic number
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	movl	%esp, %eax
	pushl	%eax
	movl	$_interrupt_handler, %eax
	call	*%eax

	popl	%eax
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popa
	addl	$8, %esp		# skip the interrupt number and error code.
	iret

.size _isr_common_stub, . - _isr_common_stub

.macro	ISR_NO_CODE	number
.global		_isr\number
_isr\number:
	cli
	pushl	$0		# Exception has no code, so we fake it
	pushl	$\number
	jmp	_isr_common_stub
.size _isr\number, . - _isr\number
.endm

.macro	ISR_WITH_CODE	number
.global		_isr\number
_isr\number:
	cli
	pushl	$\number
	jmp	_isr_common_stub
.size _isr\number, . - _isr\number
.endm

# IRQs are mapped to interrupts 32 through 47:
.macro	IRQ_NO_CODE	irq, int
.global		_irq\irq
_irq\irq:
	cli
	pushl	$\irq
	pushl	$\int
	jmp	_isr_common_stub
.size _irq\irq, . - _irq\irq
.endm

# Intel reserved interrupts (processor exceptions).  Some push an error code
# onto the stack, some don't (so we push 0 to keep all interrupts consistent).

ISR_NO_CODE 	0		# Divide by zero
ISR_NO_CODE 	1		# Debug Exception (single trace?)
ISR_NO_CODE 	2		# NMI
ISR_NO_CODE 	3		# Break point
ISR_NO_CODE	4		# INTO
ISR_NO_CODE	5		# Out of bounds
ISR_NO_CODE	6		# Invalid op code
ISR_NO_CODE	7		# Coprocessot not available
ISR_WITH_CODE	8		# Double fault
ISR_NO_CODE	9		# Co-processor segment over-run
ISR_WITH_CODE	10		# Bad TSS Exception
ISR_WITH_CODE	11		# Segment not present
ISR_WITH_CODE	12		# Stack fault
ISR_WITH_CODE	13		# GPF
ISR_WITH_CODE	14		# Page Fault Exception
ISR_NO_CODE	15		# Reserved
ISR_NO_CODE	16		# Floating point exception
ISR_NO_CODE	17		# Alignment check exception
ISR_NO_CODE	18		# Machine check exception

.irp	number	19,20,21,22,23,24,25,26,27,28,29,30,31
ISR_NO_CODE	\number		# Reserved
.endr

.irp	number	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
IRQ_NO_CODE	\number, \number+32
.endr

#############################################################################
#
#	extern void switch_stacks(uint32 new_esp, uint32 *old_esp);
#
#	Theory of operation:  Save current thread's registers onto stack as
#	if an interrupt was generated.  Swap stacks, and undo the pushes,
#	as if returning from an interrupt.

.global _switch_stacks
_switch_stacks:
// Create a stack frame.
	pushl	%ebp
	movl	%esp, %ebp

// Save old task's callee-save registers.
	pushl	%ebx
	pushl	%esi
	pushl	%edi

// Save current stack pointer.
	movl	12(%ebp), %eax
	movl	%esp, (%eax)

// Load new stack (esp)
	movl	8(%ebp), %esp

// Restore new task's callee-saved registers.
	popl	%edi
	popl	%esi
	popl	%ebx

// Exit.
	popl	%ebp
	ret

.size _switch_stacks, . - _switch_stacks
