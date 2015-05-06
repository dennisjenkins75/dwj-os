#       loader.asm -> loader.com
#
#       This file is the second stage boot loader.  It can be loaded from
#       MS-DOS (FreeDOS if you prefer), DEBUG.COM, our own boot loader
#       (which can load it to an essentially random segment:offset).
#
#       No matter how it is loaded, it should normalize its segment,
#       copy itself to 9000:0 (it will assume that no part of it is ever
#       directly loaded that high) and jump to 9000:start
#
#       Assumes that it is loaded at a segment boundary.

.equ            RELOC_SEG, 0x9000

.code16
.text

.global	_start
_start:

# On entry, DL should contain BIOS boot device id (0 = A:, 128 = C:),
# No other assumptions are made.

        call    get_ip
get_ip: 
        popw    %ax             # we just popped IP :)
        subw    $get_ip, %ax	# determine what IP was when we began executing.
        movw    %ax,%si         # save original IP

        movw    $4, %cx		# want to divide by 16
        shrw    %cl,%ax         # divide IP by 16 to get effective segment.

        pushw	%cs
        popw    %bx
        addw    %ax,%bx         # BX = abs segment we loaded at.

        movw    %bx,%ds         # DS = load segment.
        xorw    %si,%si         # DS:SI = normalized abs load address.

        movw    $RELOC_SEG, %bx
        movw    %bx,%es
        xorw    %di,%di         # ES:DI points to abs target address.

        xorw    %cx,%cx
        decw    %cx             # Move 64K. (Does this really move 64K - 1 ?? )
        rep
        
        movsb

	jmpl	$RELOC_SEG, $start

start: 

# Set our stack and then re-enable interrupts.
        movw    %bx,%ds                 # Still holds "0x9000".
        movw    %bx,%ss
        xorw    %bx,%bx
        decw    %bx
        movw    %bx,%sp                 # SS:SP = 9000:ffff
        sti

        movb    %dl,boot_dev_id         # save BIOS boot device.

        movw    $str_loaded, %si
        call    lr_print_str

        movw    %cs,%ax
        call    lr_print_hex_word

        movw    $str_crlf, %si
        call    lr_print_str

        movw    $str_booted, %si
        call    lr_print_str

        movb    boot_dev_id,%al
        call    lr_print_hex_byte

        movw    $7, %bx
        movw    $0x0e20, %ax           # ASCII space
        int     $0x10

        movb    boot_dev_id,%al
        cmpb    $0x80, %al
        jl      hd_boot
        addb    $0x41, %al             # ASCII 'A'
        jmp     print_boot
hd_boot: 
        andb    $0x7f, %al
        addb    $0x43, %al             # ASCII 'C'
print_boot: 
        int     $0x10                   # print the boot device DOS name.

        movb    $0x3a, %al             # ASCII ':'
        int     $0x10

        movw    $str_crlf, %si
        call    lr_print_str

	call	lr_get_mem_map
	call	lr_e820_dump

	call	init_loader		# C code entry point.

	movw	$str_halted, %si
	call	lr_panic

halt_1: 
	hlt
        jmp halt_1

# Sample function to try to integrate C w/ ASM

#	void	testfunc(void)
.global		testfunc
testfunc:
	movw	$str_testfunc, %si
	call	lr_print_str
	ret



.data
str_loaded:     .asciz "\r\nStage 1 boot loader relocated to: 0x"
str_crlf:       .asciz "\r\n"
str_booted:     .asciz "Booted from BIOS device 0x"
str_halted:	.asciz "Halted.\r\n"
str_testfunc:	.asciz "inside _testfunc()\r\n"

#       Our uninitialized variables:

.bss
.global		boot_dev_id
boot_dev_id:    .byte   0
