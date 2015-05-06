/*	tools/core-debugger/init.c	*/

#include "core-debugger.h"

#include "tmp/makemap.inc"

static uint8	*core = NULL;
static uint32	core_size = 0;

// 1<<20 entries.  Fully decoded page table entries.
// Unmapped pages are NULL.
static uint32	*page_tables = NULL;

// Pointer to loaded corehelp struct.
struct corehelp *corehelp = NULL;

const char*	ResolveSymbol(uint32 virtaddr)
{
	const struct symbol_t *s;

	for (s = symbols; s->name; s++)
	{
		if (s->virt_addr < virtaddr)
		{
			if (!(s+1)->virt_addr || ((s+1)->virt_addr > virtaddr))
			{
				return s->name;
			}
		}
	}

	return "?";
}

uint8		ReadByte(uint32 virtaddr)
{
	if (page_tables[virtaddr >> 12])
	{
		uint32 offset = page_tables[virtaddr >> 12] | (virtaddr & 0xfff);

		if (offset < core_size)
		{
			return core[offset];
		}
	}

	return 0;
}

uint32		ReadDword(uint32 virtaddr)
{
	if (page_tables[virtaddr >> 12])
	{
		uint32 offset = page_tables[virtaddr >> 12] | (virtaddr & 0xfff);

		if (offset < core_size)
		{
			return *(uint32*)(core + offset);
		}
	}

	return 0;
}

char	*DupString(uint32 virtaddr)
{
	int	len = 0;
	char	*str = NULL;

	for (len = 0; ReadByte(virtaddr + len); len++);
	str = malloc(++len);
	for (len = 0; 0 != (str[len] = (char)ReadByte(virtaddr + len)); len++);
	str[len] = 0;

	return str;
}

static void	BuildPageTables(uint32 cr3)
{
	int	i;
	uint32	*page_table;
	uint32	*page_dir;
	int	pde;
	int	pte;

	if (cr3 > core_size)
	{
		fprintf(stderr, "Error: CR3(%08x) > core_size (%08x)\n", cr3, core_size);
		exit(-1);
	}

	i = sizeof(uint8*) * (1 << 20);
	if (NULL == (page_tables = (uint32*)malloc(i)))
	{
		fprintf(stderr, "malloc(%d) failed.\n", i);
		perror("malloc");
		exit(-1);
	}

	page_dir = (uint32*)(core + cr3);

	for (pde = 0; pde < 1024; pde++)
	{
		if (page_dir[pde] & 0x00000001)		// page present bit.
		{
			i = page_dir[pde] & 0xfffff000;

			if (i > core_size)
			{
				fprintf(stderr, "Error: page_dir[%03x] = %08x.  core_size = %08x.\n", pde, i, core_size);
				continue;
			}

			page_table = (uint32*)(core + i);

			for (pte = 0; pte < 1024; pte++)
			{
				if (page_table[pte] & 0x00000001)
				{
					page_tables[pde * 1024 + pte] = page_table[pte] & 0xfffff000;
				}
			}
		}
	}

/*
	for (i = 0; i < 1024 * 1024; i++)
	{
		if (page_tables[i])
		{
			printf("%08x -> %08x\n", i << 12, page_tables[i]);
		}
	}
*/
}

void	LoadCore(const char *filename)
{
	struct stat	statbuf;
	int		fd;
	int		bytes;
	uint32		offset = 0;
	uint32		left;
	uint32		cr3 = 0;

	if (-1 == (fd = open(filename, O_RDONLY)))
	{
		fprintf(stderr, "Failed to open '%s' for reading.\n", filename);
		perror("open");
		exit(-1);
	}

	if (-1 == fstat(fd, &statbuf))
	{
		fprintf(stderr, "stat failed.\n");
		perror("stat");
		exit(-1);
	}

	core_size = statbuf.st_size;

	if (NULL == (core = (uint8*)malloc(core_size)))
	{
		fprintf(stderr, "malloc(%d) failed.\n", core_size);
		perror("malloc");
		exit(-1);
	}

	while (offset < core_size)
	{
		left = core_size - offset;

		if (0 == (bytes = read(fd, core + offset, left)))
		{
			fprintf(stderr, "premature EOF on %s\n", filename);
			exit(-1);
		}

		if (bytes == -1)
		{
			fprintf(stderr, "error reading %d bytes from %s\n", left, filename);
			perror("read");
			exit(-1);
		}

		offset += bytes;
	}

	close(fd);

// Scan for "corehelp"
	for (offset = 0; offset < core_size - sizeof(*corehelp); offset += 4)
	{
		corehelp = (struct corehelp*)(core + offset);

		if ((corehelp->magic1 == CORE_HELP_MAGIC1) && (corehelp->magic2 == CORE_HELP_MAGIC2) && (sizeof(*corehelp) == corehelp->size))
		{
			fprintf(stderr, "Located 'corehelp' structure at VM Phys %08x\n", (uint32)corehelp - (uint32)core);
			break;
		}
	}

	if ((corehelp->magic1 != CORE_HELP_MAGIC1) || (corehelp->magic2 != CORE_HELP_MAGIC2))
	{
		fprintf(stderr, "Unable to locate 'corehelp' structure.\n");
		exit(1);
	}

	cr3 = *(uint32*)(&corehelp->cr3);
	printf("cr3 = %08x\n", cr3);

	BuildPageTables(cr3);
}
