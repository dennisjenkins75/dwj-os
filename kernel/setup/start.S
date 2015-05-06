#       kernel/start.S
#
#	This file MUST be the first file linked into the kernel.

.code32

.global __setup_start, __setup_end
.global	__text_start, __text_end
.global __data_start, __data_end
.global __bss_start, __bss_end

.section .setup, "awx"
__setup_start:

.text
__text_start:

.data
__data_start:

.bss
__bss_start:


##################################################################
#
#
#	This GRUB structure must appear within the first 8K of the kernel.
#	The linker script will handle this for us.

.section .setup, "awx"
.align 4
mboot:
	.equ	MULTIBOOT_PAGE_ALIGN, 1<<0
	.equ	MULTIBOOT_MEMORY_INFO, 1<<1
	.equ	MULTIBOOT_HEADER_MAGIC, 0x1badb002
	.equ	MULTIBOOT_HEADER_FLAGS, MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
	.equ	MULTIBOOT_CHECKSUM, -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

# GRUB will scan the first 8K of the kernel to find this boot signature.
	.long	MULTIBOOT_HEADER_MAGIC
	.long	MULTIBOOT_HEADER_FLAGS
	.long	MULTIBOOT_CHECKSUM


###############################################################################
#
#	Boot loader jumps to our code here.
#	On entry:
#		EAX = boot-loader magic value.
#		EBX = physical address of multiboot_info struct
#		No paging, flat segments, must define own stack.

.global	_start
_start:

# Debugging aid.  Print "0" in white on red at VGA cell 0.
	movw	$0x4f30, (0x0b8000)

	cmpl	$0x2BADB002, %eax
	je	boot_ok

	cli
	hlt

boot_ok:

# Initialize our temporary stack.
	movl	$_setup_stack, %esp

# Save multiboot_info_t pointer
	pushl	%ebx

	pushl	%ebx
	call	__setup_init_console
	popl	%ebx

# Call C code to initialize the first page tables.
# extern void _setup_init_paging func(const multiboot_info_t *mbi);
	pushl	%ebx
	call	__setup_init_paging

# Restore multiboot_info_t pointer.
	popl	%ebx

# Now that our kernel is properly page mapped, rebuild the stack.
# The .setup.data section (holding previous stack) will be returned
# to the physical memory allocator shortly.
#	movl	$__kernel_stack, %esp
	movl	$__kernel_stack_start, %eax
	addl	$__kernel_stack_size, %eax
	subl	$4, %eax
	movl	%eax, %esp

# Grub sets "EBX" to point to the mutliboot_info structure.
# Pass it to our kernel's C code startup routine.

	pushl	%ebx
	call	_kmain

# kmain() should never return.  If it tries to, the page that used to be
# here (.setup.text) will no longer exist.

.size _start, . - _start

###################################
#
# Define the setup stack.  Once setup is over,
# this page will be reclaimed for general use.
#
# Yes, this stack IS in the .setup.text section.
# It is not marked 'read-only'.

.align 4
	.space	512
_setup_stack:
