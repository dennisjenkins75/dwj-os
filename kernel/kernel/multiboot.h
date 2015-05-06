/* 	kernel/multiboot.h

	Contains stuff related to how the kernel boots up, how to
	get the GRUB multi-boot structure.
*/

#ifndef __MULTIBOOT_H__
#define __MULTIBOOT_H__

#define CHECK_MULTIBOOT_FLAG(flags, bit)  ((flags) & (1 << (bit)))

// The magic number for the Multiboot header.
// COULD ALSO BE 0x2BADB002 ACCORDING TO GRUB DOCS
#define MULTIBOOT_MAGIC          0x2BADB002
#define MULTIBOOT_MAGIC_ALTERN   0x1BADB002

// The flags for the Multiboot header.
#ifdef __ELF__
# define MULTIBOOT_HEADER_FLAGS         0x00000003
#else
# define MULTIBOOT_HEADER_FLAGS         0x00010003
#endif

#define SMAP		0x534d4150		// 'SMAP'
#define E820_RECS	128

struct	e820_rec
{
	unsigned long long addr;
	unsigned long long size;
	unsigned long type;
};

// The Multiboot header.
struct phys_multiboot_header
{
	unsigned long magic;
	unsigned long flags;
	unsigned long checksum;
	unsigned long header_addr;
	unsigned long load_addr;
	unsigned long load_end_addr;
	unsigned long bss_end_addr;
	unsigned long entry_addr;
};

// The symbol table for a.out.
struct phys_aout_header
{
	unsigned long tabsize;
	unsigned long strsize;
	unsigned long addr;
	unsigned long reserved;
};

// The section header table for ELF.
struct phys_elf_header
{
	unsigned long num;
	unsigned long size;
	unsigned long addr;
	unsigned long shndx;
};

// The module structure.
struct phys_mb_module
{
	unsigned long mod_start;
	unsigned long mod_end;
	unsigned long string;
	unsigned long reserved;
};

// Memory range types.
#define E820_AVAIL	1
#define E820_RESERVED	2
#define E820_ACPI	3
#define E820_ACPI_NVS	4

// The memory map. Be careful that the offset 0 is base_addr_low but no size.
struct e820_memory_map
{
	unsigned long size;
	unsigned long base_addr_low;
	unsigned long base_addr_high;
	unsigned long length_low;
	unsigned long length_high;
	unsigned long type;
};

// The Multiboot information.
struct phys_multiboot_info
{
	unsigned long flags;
	unsigned long mem_lower;
	unsigned long mem_upper;
	unsigned long boot_device;
	unsigned long cmdline;
	unsigned long mods_count;
	unsigned long mods_addr;
	union
	{
		struct phys_aout_header		aout_sym;
		struct phys_elf_header		elf_sec;
	} u;
	unsigned long mmap_length;
	struct e820_memory_map_t *mmap_addr;
};

// We rebuild what grub gave us in this form:

struct mb_module
{
	void	*start_virt;
	void	*start_phys;
	uint32	size;
	char	*string;
};

struct mb_elf_header
{
	unsigned long num;
	unsigned long size;
	unsigned long addr;
	unsigned long shndx;
};

struct  multiboot
{
	uint32                  flags;
	uint32                  mem_lower;
	uint32                  mem_upper;
	uint32                  boot_device;
	char                    *cmdline;

	uint32                  mods_count;
	struct mb_module	*mods_array;

	uint32                  elf_hdr_count;
	struct mb_elf_header	*elf_headers;

	uint32                  e820_count;
	struct e820_memory_map	*e820_table;
};

extern struct multiboot		*gp_MultiBootInfo;

void	dump_mboot_info(const struct phys_multiboot_info *mbi);
void    relocate_mbi(const struct phys_multiboot_info *mbi);

#endif // __MULTIBOOT_H__

