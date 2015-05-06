# dumpbios.S

#       Boot sector that just dumps the registers and then halts.

.code16
.text
.global	_start

_start:
	jmp	skip_bpb


	# DOS and BIOS parameter block goes here.
	# Our buidl script will use "mkdosfs" to fill it in.
	# See the Makefile for details.

.org	62
skip_bpb:

        movw    $0x4353, %dx   # 'CS'
        pushw   %cs
        popw    %ax
        call    print_reg

        movw    $0x4453, %dx   # 'DS'
        pushw   %ds
        popw    %ax
        call    print_reg

        movw    $0x4553, %dx   # 'ES'
        pushw   %es
        popw    %ax
        call    print_reg

        movw    $0x5353, %dx   # 'SS'
        pushw   %ss
        popw    %ax
        call    print_reg

        call    get_ip
get_ip: 
        popw    %ax
	subw	$get_ip, %ax
        movw    $0x4950, %dx   # 'IP'
        call    print_reg

        movw    %sp,%ax
        movw    $0x5350, %dx   # 'SP'
        call    print_reg

halt_x: 
	hlt
        jmp     halt_x

# reg name in 'dx'
# reg value in 'ax'
print_reg: 
        pushw   %ax
        movw    $7, %bx
        movb    $0x0e, %ah
        movb    %dh,%al
        int     $0x10
        movb    %dl,%al
        int     $0x10
        movb    $0x20, %al
        int     $0x10
        popw    %ax
        call    print_hex_word

print_crlf: 
        pushw   %ax
        pushw   %bx
        movw    $7, %bx
        movw    $0x0e0a, %ax
        int     $0x10
        movb    $0x0d, %al
        int     $0x10
        popw    %bx
        popw    %ax
        ret


# ########################################################
#
#       print_str
#
# On input, DS:SI points to ASCIIZ string to print via BIOS.
# all registers are saved.
print_str: 
        pusha
        movb    $0x0e, %ah
        movw    $7, %bx
print_str_loop: 
        lodsb
        orb     %al,%al         # did we just load the nil?
        jz      print_done
        int     $0x10
        jmp     print_str_loop
print_done: 
        popa
        ret

##########################################################
#
# print 'ax' as 4 hex digits.
# destroys ax
# BX must be properly set for BIOS.

print_hex_word: 
        pushw   %ax
        movb    %ah,%al
        call    print_hex_byte
        popw    %ax

# print 'al' as 2 hex digits.
print_hex_byte: 
        pushw   %ax
        shrb    $4, %al
        call    print_hex_nibble
        popw    %ax

# print lower nibble of 'al' as 1 hex digit.
print_hex_nibble: 
        pushw   %ax
        andb    $0x0f, %al
        cmpb    $0x09, %al
        jle     prn_nibble_1
        addb    $39, %al       # 'a' - '9'
prn_nibble_1: 
        addb    $48, %al
	pushw	%bx
	movw	$7, %bx
        movb    $0x0e, %ah
        int     $0x10
	popw	%bx
        popw    %ax
        ret


#	Boot signature.
.org    510
	.word	0xAA55

