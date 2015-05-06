#	loader/memmap.S
#
#	Get BIOS memory map, cache where our future protected mode code can find it.

.equ	SMAP,		0x534d4150
.equ	E820_MAX_RECS, 	128
.equ	E820_REC_SIZE,	20

.code16

.bss
.global		e820_map
.global		e820_count
e820_count:	.byte		0
e820_map:	.rept		E820_MAX_RECS
		  .rept		E820_REC_SIZE
		    .byte	0
		  .endr
		.endr

#	lr_get_mem_map
#
#	Uses ACPI BIOS 2.0 interface (int 15h, AX=e820) to read system memory map.
#	http://www.acpi.info
#
#	Saves all registers.  Stores array and count in the globals above.

.text
.global		lr_get_mem_map
lr_get_mem_map:

# Save all registers that we will destory.
	pushw	%es
	pushl	%esi
	pushl	%edi
	pushl	%eax
	pushl	%ebx
	pushl	%ecx
	pushl	%edx

	movw	$str_e820_probe, %si
	call	lr_print_str

	pushw	%ds
	popw	%es

	xorl	%ebx, %ebx		# used by BIOS.
	xorl	%esi, %esi		# counts recs stored.
	movw	$e820_map, %di		# Store rec here.
e820_next:
	movl	$20, %ecx		# Rec size.
	movl	$SMAP, %edx		# Magic value.
	movl	$0x0000e820, %eax	# Service #
	int	$0x15			# Ask ACPI BIOS for mem map.
	jc	e820_done
	cmpl	$SMAP, %edx		# Sanity check
	jne	e820_done
	orl	%ebx, %ebx		# ACPI documented end-of-list
	jz	e820_done

	addw	$20, %di
	addw	$1, %si
	cmpw	$E820_MAX_RECS, %si
	jl	e820_next

e820_done:
	movw	%si, e820_count

	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	popl	%edi
	popl	%esi
	popw	%es

	ret

#################################################
#
#	e820_dump
#
#	Dumps e820 memory map to video BIOS.
#	Used for debugging.
#	Saves all modified registers.

.global	lr_e820_dump
lr_e820_dump:
	pushw	%si			# Printing strings, reading memory.
	pushw	%ax			# generic
	pushw	%bx			# Used by BIOS for video mode flags.
	pushw	%cx			# Counts recs to print

	movw	e820_count, %cx		# Want count in CX
	movw	$e820_map, %si		# Want offset of table is SI
	movw	$7, %bx			# BIOS video flags.

rec_loop:
	# Addresses and Lengths are 64 bits.
	pushw	%si			# save offset for later use.

	pushw	%si			# print some spaces.
	movw	$str_space, %si
	call	lr_print_str
	popw	%si

	call	lr_print_hex_qword	# print base address

	movw	$0x0e20, %ax
	int	$0x10			# print space.

	addw	$8, %si
	call	lr_print_hex_qword	# print length

	movw	$0x0e20, %ax
	int	$0x10			# print space.

	addw	$8, %si
	lodsb				# get data type
	pushw	%ax
	addw	$0x30, %ax		# convert to ASCII decimal.
	int	$0x10			# print.

	movb	$0x20, %al
	int	$0x10			# print space.

	popw	%ax			# restore memory range type.  Print human text for it.
	xorb	%ah, %ah
	cmpb	$0, %al
	jle	type_unk
	cmpb	$4, %al
	jg	type_unk
	movw	$acpi_table, %si
	sub	$1, %ax			# ACPI types start at 1, our array at zero.
	shlw	$1, %ax			# AX is index into table of words.
	addw	%ax, %si		# SI is index into array of string pointers.
	mov	(%si), %si		# SI is now pointer to char array.
	jmp	print_type

type_unk:
	movw	$str_type_unk, %si

print_type:
	call	lr_print_str

	movw	$str_crlf, %si
	call	lr_print_str

	popw	%si
	add	$E820_REC_SIZE, %si

	loop	rec_loop


	popw	%cx
	popw	%bx
	popw	%ax
	popw	%si

	ret

.data
str_crlf:	.asciz "\r\n"
str_e820_probe:	.asciz "Probing BIOS (int 15h, ax=e820h) for memory map:\r\n"
str_space:	.asciz "    "

str_type_1:	.asciz "available"
str_type_2:	.asciz "reserved"
str_type_3:	.asciz "acpi"
str_type_4:	.asciz "acpi-nvs"
str_type_unk:	.asciz "unknown"

acpi_table:	.word str_type_1
		.word str_type_2
		.word str_type_3
		.word str_type_4
