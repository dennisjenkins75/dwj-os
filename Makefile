# Makefile for dwjos
#
# Required tool chain:
#	GCC
#	sys-fs/dosfstools	# not needed anymore??
#	sys-fs/mtools		# for building the boot floppy.
#	app-cdr/cdrtools	# for mkisofs, for making the boot cd.

#
# You can edit these settings.
#
OUT_DIR:=	./output
TMP_DIR:=	./tmp

#
# It is best to leave these alone:
#
GAS:=as
MKDOSFS:=/usr/sbin/mkdosfs
GRUB_SRC_DIR:=/boot/grub

# #osdev, "froggey"'s c flags:
# -nostdinc -Iinclude -ffreestanding -Wall -Wextra -Wcast-align -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wstrict-prototypes -O2 -march=pentium4 -mtune=pentium4 -g -std=gnu99 

CFLAGS:=

KERNEL_CFLAGS:=-DBUILDING_KERNEL -Werror -Wall -std=c99 -O0 -m32 -I ./ -I ./kernel/ -nostdinc \
	-fno-strength-reduce -finline-functions -fno-builtin -fleading-underscore -ffreestanding \
	-Wno-unused-but-set-variable -Wno-unused-variable
##	-fstrength-reduce -finline-functions -fno-builtin -fleading-underscore -ffreestanding

HOST_CFLAGS:=-Wall -O2 -I ./

ASFLAGS:=	--32

# Place all compiler intermediates here:
#OBJ_DIR:=	$(TMP_DIR)/obj
OBJ_DIR:=	.

#
# Aliases for the targets:
#

GRUB_CDROM:=	$(OUT_DIR)/grub_cd.iso
GRUB_FLOPPY:=	$(OUT_DIR)/grub_fd0.img
FLOPPY:=	$(OUT_DIR)/floppy.img
DEBUG:=		$(OUT_DIR)/debug.img
DEBUGGER:=	$(OUT_DIR)/core-debugger

GRUB_MENU:=	$(TMP_DIR)/grub_menu_fd0.lst
GRUB_MENU_CD:=	$(TMP_DIR)/grub_menu_cd.cd
BOOT.0.fat12:=	$(TMP_DIR)/boot.0.fat12
LOADER.COM:=	$(TMP_DIR)/loader.com
DEBUG.0:=	$(TMP_DIR)/debug.0
KERNEL.MAP:=	$(TMP_DIR)/kernel.map
KERNEL.ELF:=	$(TMP_DIR)/kernel.elf
TEST_MOD:=	$(TMP_DIR)/test_mod.bin
ISO_DIR:=	$(TMP_DIR)/iso
MAP_TEMP:=	$(TMP_DIR)/makemap.inc
MAKE_MAP_TOOL:=	$(TMP_DIR)/makemap
KERNEL.VAST:=	$(TMP_DIR)/kernel.vst
INITRD:=	$(TMP_DIR)/initrd.tar

#
# Aliases for misc sources, tools.
#

ELF_LINK_SCRIPT:=	kernel/kernel/kernel-elf.lds


#
# File system objects that we build.
#

# Disk Images
IMAGES:=	$(GRUB_FLOPPY) $(GRUB_CDROM)

# Binaries
TARGETS:=	$(TEST_MOD) $(MAP_TEMP) $(KERNEL.ELF) $(KERNEL.VAST) $(INITRD)

# Tools
TOOLS:=		$(MAKE_MAP_TOOL) $(DEBUGGER)

#
# Guts of the Makefile.
#

.PHONY:		all clean dirs

all:		dirs $(TOOLS) $(TARGETS) $(IMAGES)

clean:
	rm -f $(TARGETS) $(IMAGES) $(TOOLS)
	find . -name "*.[oa]" | xargs rm -f
	find . -name "gmon.out" | xargs rm -f
	find . -name "*.lst" | xargs rm -f
	rm -rf $(TMP_DIR) ./output

dirs:
	@mkdir -p $(OUT_DIR)
	@mkdir -p $(TMP_DIR)
	@mkdir -p $(ISO_DIR)
	@mkdir -p $(OBJ_DIR)/kernel

#
# Build rules for various targets.
#

$(GRUB_MENU): boot/grub-fd0.conf
	rm -f $@
#	echo -e "title\tdwjos\n\troot\t(fd0)\n\tkernel\t/kernel.bin\n\n" > $@
	cp boot/grub-fd0.conf $@

$(GRUB_FLOPPY):		$(GRUB_MENU) $(TARGETS) $(INITRD)
	rm -f $@
	bzcat < boot/grub.img.bz2 > $@
	mcopy -i $@ -o -t $(GRUB_MENU) ::boot/grub/menu.lst
	mcopy -i $@ -o -b $(KERNEL.ELF) ::kernel.elf
	mcopy -i $@ -o -b $(TEST_MOD) ::test_mod.bin
	mcopy -i $@ -o -b $(KERNEL.VAST) ::kernel.vst
	mcopy -i $@ -o -b $(INITRD) ::initrd.tar
	@echo -e "\n\tBootable floppy image ready: $(GRUB_FLOPPY)\n"

$(GRUB_MENU_CD):	$(GRUB_MENU)
	rm -f $@
	sed -e "s/fd0/cd/g" < $< > $@

$(GRUB_CDROM):		$(GRUB_MENU_CD) $(TARGETS) $(INITRD)
	rm -f $@
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(GRUB_SRC_DIR)/stage2_eltorito $(ISO_DIR)/boot/grub
	cp $(GRUB_MENU_CD) $(ISO_DIR)/boot/grub/menu.lst
	cp $(KERNEL.ELF) $(ISO_DIR)
	cp $(TEST_MOD) $(ISO_DIR)
	cp $(KERNEL.VAST) $(ISO_DIR)
	cp $(INITRD) $(ISO_DIR)
	mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o $@ $(ISO_DIR)
	@echo -e "\n\tBootable iso image ready: $(GRUB_CDROM)\n"

$(FLOPPY):	$(BOOT.0.fat12)
	rm -f $@
	dd if=/dev/zero of=$@ bs=512 count=2880 &> /dev/null
	$(MKDOSFS) -F 12 -n dwjosboot $@
	dd if=$(BOOT.0.fat12) of=$@ bs=1 skip=62 seek=62 count=448 conv=notrunc &> /dev/null

$(DEBUG):	$(DEBUG.0)
	rm -f $@
	dd if=/dev/zero of=$@ bs=512 count=2880 &> /dev/null
	$(MKDOSFS) -F 12 -n dwjosboot $@
	dd if=$(DEBUG.0) of=$@ bs=1 skip=62 seek=62 count=448 conv=notrunc &> /dev/null
	mcopy -t -i $@ boot/dumpbios.S ::dumpbios.S

$(BOOT.0.fat12):	boot/fat12.o
	$(LD) -Ttext 0x0 -s --oformat binary -o $@ $<

$(DEBUG.0):		boot/dumpbios.o
	$(LD) -Ttext 0x0 -s --oformat binary -o $@ $<

$(LOADER.COM):	loader/start.o loader/init.o loader/panic.o loader/print.o \
		loader/memmap.o
	$(LD) -Ttext 0x0 -s --oformat binary -o $@ $^

# Just build a dummy initial ram disk for now.
$(INITRD): $(KERNEL.VAST)
	tar vcf $(INITRD) --exclude ".svn" $(KERNEL.VAST) boot

#############################
##

KERNEL_SETUP:=	start setup_con setup_vmm
KERNEL_ARCH:=	breakpoint gdt i386 idt intr
KERNEL_DRVRS:=	ata console keyboard pci reboot timer vgafonts vmw_gate vmwguest
KERNEL_FS:=	devfs mount ramfs vfs vfs_ops vnode
KERNEL_KERNEL:=	main modules multiboot panic spinlock task obj_array semaphore wait
KERNEL_KTASKS:=	demo hud reaper startup
KERNEL_LIB:=	lib strerror
KERNEL_VMM:=	heap pagefault vmm


KERNEL_FILES:=	$(addprefix setup/,$(KERNEL_SETUP)) \
		$(addprefix arch/,$(KERNEL_ARCH)) \
		$(addprefix drivers/,$(KERNEL_DRVRS)) \
		$(addprefix fs/,$(KERNEL_FS)) \
		$(addprefix kernel/,$(KERNEL_KERNEL)) \
		$(addprefix ktasks/,$(KERNEL_KTASKS)) \
		$(addprefix lib/,$(KERNEL_LIB)) \
		$(addprefix vmm/,$(KERNEL_VMM)) \

KERNEL_FILES2:=	$(addprefix $(OBJ_DIR)/kernel/,$(KERNEL_FILES))
KERNEL_OBJ:=	$(KERNEL_FILES2:=.o)

$(KERNEL.ELF):	$(KERNEL_OBJ)
	$(LD) -m elf_i386 -T $(ELF_LINK_SCRIPT) --cref -Map $(KERNEL.MAP) -o $@ $^

$(MAKE_MAP_TOOL):	tools/makemap.c $(MAP_TEMP) $(KERNEL.MAP)
	$(CC) $(HOST_CFLAGS) -o $@ tools/makemap.c

$(MAP_TEMP):	$(KERNEL.MAP) $(KERNEL.ELF)
	perl -w scripts/proc_map.pl < $(KERNEL.MAP) > $(MAP_TEMP)

$(KERNEL.VAST):	$(TMP_TEMP) $(MAKE_MAP_TOOL) $(KERNEL.MAP) $(KERNEL.ELF)
	$(MAKE_MAP_TOOL) > $@

$(TEST_MOD):	mod/test.o
	cp $< $@

.s.o:
	$(AS) $(ASFLAGS) -I inc -o $@ $<

.S.o:
	$(AS) $(ASFLAGS) -I inc -o $@ $<

.c.o:
	$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

# Dependencies.
$(KERNEL.ELF):	$(ELF_LINK_SCRIPT)
$(KERNEL.MAP):	$(KERNEL.ELF)

#############################################################################
#

DEBUGGER_FILES:=	heap init main stack
DEBUGGER_PATHS:=	$(addprefix tools/core-debugger/,$(DEBUGGER_FILES))
DEBUGGER_SRC:=		$(DEBUGGER_PATHS:=.c)
DEBUGGER_OBJ:=		$(DEBUGGER_PATHS:=.o)

$(DEBUGGER_SRC):	tools/core-debugger/core-debugger.h

$(DEBUGGER): $(DEBUGGER_OBJ) | $(KERNEL.MAP) $(MAP_TEMP)
	$(CC) $(HOST_CFLAGS) -o $@ $(DEBUGGER_OBJ)

tools/core-debugger/%.o : tools/core-debugger/%.c
	$(CC) $(HOST_CFLAGS) -c -o $@ $<


