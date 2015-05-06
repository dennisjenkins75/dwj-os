#       loader/panic.S
#

.code16
.text

##########################################################
#
#	lr_panic - Halt real-mode loader.
#	input: ds:si points to panic string.
#	If loader was started from DOS, then return to DOS.

.global		lr_panic
lr_panic:
	pushw	%cs
	popw	%ds			# Reset DS.  God knows how fucked up it is.

	pushw	%si
	movw	$str_panic, %si
	call	lr_print_str

	popw	%si
	call	lr_print_str

# Determine if we are loaded from DOS or raw boot floppy.
	pushw	%ds
	xorw	%ax, %ax
	movw	%ax, %ds	# Point DS at interrupts.

	movw	(4 * 0x21), %si		# DOS int 20h
	movw	2(%si), %ax		# Get segment for int
	movw	(%si), %bx		# Get offset

	popw	%ds

	or	%ax, %bx		# Neat trick.  Any bits in ax or bx?
	jz	no_dos			# Nope, then DOS is not loaded.

# Print "int 21h = xxxx:yyyy\n"
	movw	$str_int21, %si
	call	lr_print_str
	call	lr_print_hex_word
	movw	$str_colon, %si
	call	lr_print_str
	movw	%bx, %ax
	call	lr_print_hex_word
	movw	$str_dos_exit, %si
	call	lr_print_str

	movw	$0x4c00, %ax		# 4c = exit, 00 = ret code.
	int	$0x21			# Disgusting, I know.

no_dos:
	movw	$str_nodos, %si
	call	lr_print_str

panic_loop:
	hlt			# Don't burn the CPU.
	jmp	panic_loop


.data

str_int21:	.asciz	"DOS int 0x21 = "
str_colon:	.asciz	":"
str_panic:	.asciz	"loader panic: "
str_dos_exit:	.asciz	", exiting to DOS (we hope).\n\r"
str_nodos:	.asciz	"DOS not loaded, nothing to exit to.  Halting.\n\r"
