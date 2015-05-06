# bootsect.S

#       RELOC_SEG is where in memory our boot loader relocates to.
#       We use the entire 64K (code at bottom, the disk buffers, stack
#       at the top).  Segment 9000 is the last 64K chunk before VGA ram.
.equ            RELOC_SEG, 0x9000

#       FAT is the offset into our CS/DS/SS segment that we load the
#       FAT (file allocation table) into.  It MUST be a multiple of 512
#       (read_linear requires it).  Can't put it at 0x0200, as that is
#       where I store my private variables.

.equ            FAT_OFFSET, 0x0400

#       16 bit code, real mode.  Yeah, it is 1981 all over again!

.code16
.text
.global		_start

_start:

# Initial JMP instruction.  These bytes won't be used when we build
# our real boot floppy.  I let mkdosfs build the floppy and then copy in
# only bytes 0x003c through 0x01fe.
	jmp	start
	nop

.org	3

# BPB (BIOS Paramater Block) begins here.

oem_name:		.ascii	"MSDOS5.0"
bytes_sector:           .word	512               # bytes per sector
sectors_cluster:        .byte	1                 # sectors per cluster
reserved_sectors:       .word	1                 # reserved sector count
num_fats:               .byte	1                 # number of FATs
root_dir_entries:       .word	0x00e0            # number of root dir entries
total_sectors:          .word	0x0b40            # total number of sectors
media_descriptor:       .byte	0xf0              # media descriptor (dsdd)
sectors_fat:            .word	9                 # sectors per FAT
sectors_track:          .word	18                # sectors per track
number_heads:           .word	2                 # number of heads
hidden_sectors:         .long	0                 # hidden sectors
total_sectors_ext:      .long	0                 # total sectors (overflow)

# BPB ends here, Extended BPB begins.

drive_number:		.byte	0                 # drive number
current_head:		.byte	0                 # unused
ext_signature:		.byte	0                 # ext boot signature
serial_number:		.long	0xdeadbeef        # serial number
volume_label:		.ascii	"NO NAME    "	# volume label
fs_type:		.ascii	"FAT12   "	# fs type

start: 

# We want to load the next stage boot loader to the same location that we are
# at right now.  So before we do anything else, let's relocate to 0x9000:0
# If BIOS loads us we are at 0000:7c00.  Some BIOSes jmpf to 07c0:0000.
# If we boot DOS and use DEBUG.COM to load the boot sector for testing, then
# we actually can't load it at 07c0:0000 (we'll destroy DOS).  So we load at
# 27c0:0000.  So either way, we must determine our absolute load address and
# get that into DS:SI.

# VMWARE:  VMware sets the registers as follows when it jumps to the boot sector:
# CS = ES = SS = 0000, DS = 0040, IP = 7c00, ss = 03ff (??)

        cld
        cli

# Determine our current IP (instruction pointer).
        call    find_ip
find_ip: 
        popw    %si                     # SI = IP of (find_ip)
        subw    $find_ip, %si          # SI = original IP when we loaded.
        pushw   %si                     # Will pop after we blit to 9000:0

        pushw   %cs
        popw    %ds                     # DS:SI should be where BIOS or 
                                        # DEBUG.COM loaded us at originally.

# Copy boot loader to 9000:0
        movw    $RELOC_SEG, %bx        # We want to relocate to here.
        movw    %bx,%es
        xorw    %di,%di
        movw    $0x0100, %cx           # move 512 bytes (256 words)
        rep
        
        movsw

# Now, save our origial CS:IP into boot_cs, boot_ip at segment 9000.
        movw    %bx,%ds                 # DS is now 9000, so we can use our private variables.
        pushw   %cs
	popw	boot_cs
	popw	boot_ip

# Set our stack and then re-enable interrupts.
        movw    %bx,%ss
        decw    %cx             # CX is already zero from the "rep movsw";
                                # Now make CX = 0xffff (top of our segment).
        movw    %cx,%sp         # Set stack to top of RELOC_SEG
        sti

	jmpl	$RELOC_SEG, $start2

start2: 
# At this point, CS = DS = ES = SS = 9000 (or we hope so!)

        movb    %dl, drive_number      # save this (dl set from BIOS).

# Calibrate the floppy disk.
        xorw    %ax,%ax                 # ah = 0 = reset function, dl = disk.
        int     $0x13

# Read entire FAT into es:0000
        movb    num_fats,%al
        movb    sectors_fat,%ah
        mulb    %ah                    # AX = sectors for all FATs.
	pushw	reserved_sectors
        addw    %ax,reserved_sectors    # now points to sector after FAT (root dir).
        xchgw   %ax,%cx                 # CX = sectors for all FATs.
        popw    %ax                     # AX = fat sector.
        movw    $FAT_OFFSET, %bx       # Load FAT into memory after our private variables.
                                        # ES = DS still.  Target = ES:BX = 9000:0400.
        call    read_linear

# Read root directory into current ES:BX (ES:BX changed by read_linear)
        movw    root_dir_entries,%di
        movb    $4, %cl                        # prep to divide by 16.
        shrw    %cl,%di                         # di = root dir sector count.
        addw    %di,reserved_sectors            # [res] now point to first sector of root dir.

read_root: 
        pushw   %di                             # preserve counter of root dir sectors.
        pushw   %es                             # save buffer address, will re-use.
        movb    $1, %cl                        # read one sector
        call    read_linear
        popw    %es                             # restore buffer address (ES:BX)

        movw    %bx,%di                         # es:di = root dir.
        movb    $32, %cl                       # 16 entries, 32 bytes per, 512 bytes total.

read_root_nextfile: 
        pushw   %cx                             # save counter
        movb    $11, %cl                       # filename is 11 bytes
        movw    $loader_name, %si
        repe
        cmpsb                                   # compare string.
        je      found_loader
        addw    %cx,%di                         # skip remainder of filename char slots.
        addw    $21, %di                       # advance to next dir entry.
        popw    %cx                             # restore counter.
        loop    read_root_nextfile

                # If we get here, we did not find the loader in this directory sector.
                # BUT, it could be in other root directory sectors.

        popw    %di                             # restore root dir sector counter.
        decw    %di
        jnz     read_root

        movw    $loader_name, %si              # prints "LOADER  COM !found" error.
        call    print_str
halt_1:
	hlt
        jmp     halt_1                          # loop forever.

found_loader: 
		# Before next instruction: ES:DI points to 11th byte of 32 byte directory entry.
		# After next instruction, DI contains first cluster of the file.
	movw	%es:15(%di), %di		# Intel syntax: "mov di, es:[di+15]"
        movw    boot_cs, %bx
        movw    %bx,%es
        movw    boot_ip, %bx

next_block: 
        xorw    %cx,%cx
        movb    sectors_cluster,%cl             # min read this many sectors.
        movw    %di,%si                         # si = next cluster to read.

next_contig: 
        movw    $3, %ax
        mulw    %si                             # assume DX = 0
        shrw    $1, %ax                        # ax = si * 3 / 2
        xchgw   %ax,%bp                         # bp = ax
	movw	FAT_OFFSET(%bp), %ax
        jc      odd_cluster

even_cluster: 
        andw    $0xfff, %ax                    # leave only lower 12 bits
        jmp     got_cluster

odd_cluster: 
        pushw   %cx                             # save sector count
        movb    $4, %cl
        shrw    %cl,%ax                         # shift down 4 bits.
        popw    %cx

got_cluster: 
        incw    %si                             # si = current cluster.
        cmpw    %si,%ax                         # is the next cluster = current + 1?
        je      still_contig

force_read: 
        xchgw   %ax,%di                         # ax = base cluster, di = new cluster
        decw    %ax
        decw    %ax                             # actual cluster is 2 less.

                # currently assuming 1 sector per cluster for floppies.
        xorw    %dx,%dx
        movb    sectors_cluster,%dl
        mulw    %dx                             # ax = sector of base cluster

        addw    reserved_sectors,%ax            # skip these
        call    read_linear                     # read CX sectors at AX to ES:BX

        cmpw    $0x0ff8,%di                     # is the next cluster EOF?
        jb      next_block

all_read: 
        movw    $str_loaded, %si
        call    print_str
        int $3

# Turn of the floppy drive motor.
	movw	$0x3f2, %dx
	xorw	%ax, %ax
	outb	%al, %dx

# Prepare to execute the newly loaded code.

        cli
        movw    boot_cs,%bx                     # cache in register.
        movw    boot_ip,%cx                     # cache in register.
        movb    drive_number,%dl                # pass to stage 1 boot loader in this register.

        movw    %bx,%ds
        movw    %bx,%es
        movw    %bx,%ss
        movw    $0xfffe, %ax                   # Use AX, don't nuke BX or CX.
        movw    %ax,%sp
        pushw   %bx                             # boot_cs
        pushw   %cx                             # boot_ip
        sti
        lret                                    # execute what we've loaded.

still_contig: 
        addb    sectors_cluster,%cl
        adcb    $0, %ch
        jmp     next_contig


# ########################################################
#
#       read_linear
#
# Inputs:
#       AX = LBA starting sector.
#       CX = count of sectors.
#       ES:BX = buffer (must be 512-byte page aligned).
# Destroys:
#       ES (points to segment after last sector read).
#       BP, SI

read_linear: 

# Notes from original bootsector author:
#read:
#       convert x (LBA starting sector) to CHS triple
#       count=SecsPerTrack-S+1
#       if count>n (number of sectors to read) then count=n
#       calculate the maximum number of sectors that can be read between the
#               physical address of es:bx and the next 64kb boundary
#       if count>number then count=number
#       read count sectors at CHS to es:bx
#       sub n,count
#       add x,count
#       if n<>0 then jmp read

read_linear_next: 
        pushw   %ax             # save LBA starting sector
        pushw   %cx             # save count.

# convert LBA to CHS
        cwtd                            # dx = 0
	divw	sectors_track
        incw    %dx                     # BIOS counts from 1.
        pushw   %dx                     # preserve it for now.
        cwtd                            # dx = 0 again.
	divw	number_heads
        popw    %cx                     # cx = sector #
        movb    %al,%ch                 # ch = bits 0-7 of track #
        movb    %dl,%dh                 # dh = head #, cl = sector bits 5-0
        movb    drive_number,%dl
        movw    sectors_track,%ax       # ah better be zero!
        subb    %cl,%al                 # al = sectors we can read at once.
        incw    %ax                     # can_read = sec_per_track - S + 1

        popw    %si                     # si = saved count of sectors.
        pushw   %si                     # save it again :)
        cmpw    %si,%ax                 # can we read more than we need?
        jna     read_linear_count_really_ok
        xchgw   %ax,%si                 # read the smaller amount. (ax = min(si, ax))

read_linear_count_really_ok: 
# Calculate the max number of sectors that we can read between the
# physical address of es:bx and the next 64kb boundary.

        pushw   %bx
        pushw   %cx
        pushw   %ax

        movw    %es,%ax
        movb    $4, %cl
        shrw    %cl,%bx
        addw    %bx,%ax                 # ax = physical address is paragraphs
        movw    %ax,%bx
        andw    $0xf000, %bx           # mask off lower 12 bits
        addb    $0x10, %bh             # increase bits 12-15.
        subw    %ax,%bx                 # bx = # paragraphs between es:bx and next boundary
        incw    %cx                     # cx is now 5 (for division by 32).
        shrw    %cl,%bx                 # bx = # 512 byte blocks.
        popw    %ax
        popw    %cx

        cmpb    %bl,%al                 # trying to read too much?
        jna     read_linear_count_ok
        xchgw   %bx,%ax                 # read up to end of boundary.

read_linear_count_ok: 
        popw    %bx
        movw    $3, %bp                # retries

read_linear_again: 
        pushw   %ax                     # ax = count
        movb    $0x02, %ah             # BIOS function = read sectors.
        int     $0x13
        jnc     read_linear_ok

        xorb    %ah,%ah                 # BIOS function (0) = recalibrate
        int     $0x13
        popw    %ax                     # restore count
        decw    %bp
        jnz     read_linear_again

# print disk error.
        pushw   %cs             # reload DS with CS
        popw    %ds
        movw    $str_disk_error, %si
        call    print_str
halt_2: 
        jmp     halt_2


read_linear_ok: 
        popw    %ax                     # restore count.
        pushw   %ax                     # and save it again.
        movb    $5, %cl                # prep to mulltiply by 32
        shlw    %cl,%ax                 # ax = # paragraphs read in.

        movw    %es,%cx
        addw    %ax,%cx
        movw    %cx,%es                 # Update ES with new paragraph value.

        popw    %si                     # restore count to SI
        popw    %cx                     # # of sectors to read
        popw    %ax                     # LBA number
        addw    %si,%ax                 # Increase LBA by # of sectors read.
        subw    %si,%cx                 # Decrease count # of sectors to read.
        jnz     read_linear_next
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


str_loaded:     .asciz "Booting...\r\n"
str_disk_error: .asciz "Disk Error"
loader_name:    .ascii "LOADER  COM"
                .asciz " !found"

.org    510
        .word	0xAA55

# My private variables.  Note that these are not loaded by BIOS.
# They exist past the end of the boot sector.  They are in RAM right
# after the boot sector though.  The FAT is loaded at location ".org 1024".
boot_cs:        .word   0x0000
boot_ip:        .word   0x0000
