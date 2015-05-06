#       loader/print.S
#
#	Contains generic printing (string, hex, decimal) routines.

.code16
.text


##########################################################
#
#	lr_print_hex_qword
#
#	Print DS:SI as 64 bit hex value (Intel byte order)
#	Saves all registers.
#
.global	lr_print_hex_qword

lr_print_hex_qword:
	pushw	%ax
	pushw	%cx
	pushw	%si
	pushf

	std				# Want lodsb to run backwards.
	movw	$8, %cx			# Print 8 bytes
	addw	$7, %si			# MSB first when printing.
prn_qw_loop:
	lodsb
	call	lr_print_hex_byte
	loop	prn_qw_loop

	popf
	popw	%si
	popw	%cx
	popw	%ax
	ret

# ########################################################
#
# print 'ax' as 4 hex digits.
# destroys ax.

.global lr_print_hex_word
.global lr_print_hex_byte
.global lr_print_hex_nibble

lr_print_hex_word: 
        pushw   %ax
        movb    %ah,%al
        call    lr_print_hex_byte
        popw    %ax

# print 'al' as 2 hex digits.
lr_print_hex_byte: 
        pushw   %ax
        shrb    $4, %al
        call    lr_print_hex_nibble
        popw    %ax

# print lower nibble of 'al' as 1 hex digit.
lr_print_hex_nibble: 
        pushw   %ax
        andb    $0x0f, %al
        cmpb    $0x09, %al
        jle     prn_nibble_1
        addb    $39, %al       # 'a' - '9'
prn_nibble_1: 
        addb    $48, %al
        pushw   %bx
        movw    $7, %bx
        movb    $0x0e, %ah
        int     $0x10
        popw    %bx
        popw    %ax
        ret

# ########################################################
#
#       print_str
#
# On input, DS:SI points to ASCIIZ string to print via BIOS.
# preserves all registers.

.global	lr_print_str

lr_print_str: 
        pushw   %ax
        pushw   %bx
        pushw   %si
        movw    $7, %bx
print_str_loop: 
        lodsb
        orb     %al,%al         # did we just load the nil?
        jz      print_done
        movb    $0x0e, %ah
        int     $0x10
        jmp     print_str_loop
print_done: 
        popw    %si
        popw    %bx
        popw    %ax
        ret
