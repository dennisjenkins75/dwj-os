/*	kernel/kernel/vast.h	*/

#define VAST_MAGIC 0x54534156	// VAST in little-endian

struct	vast_hdr_t
{
	uint32		magic;		// 'VAST' - Virtual address symbol table.
	uint32		count;		// # of entries.
};

struct	vast_addr_t
{
	uint32		virt_addr;
	uint32		name_offset;	// From absolute beginning of symbol table.
};
