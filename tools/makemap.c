/*	tools/makemap.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int uint32;

#include "kernel/kernel/vast.h"

struct	symbol_t
{
	uint32		virt_addr;
	const char	*name;
};

// static const struct symbol_t symbols[];
#include "tmp/makemap.inc"

int	main(int argc, char *argv[])
{
	int			offset = 0;
	int			count = 0;
	FILE			*fp = stdout;
	struct vast_addr_t	*array = NULL;
	struct vast_hdr_t	hdr;

	for (count = 0; symbols[count].name; count++);
	hdr.magic = VAST_MAGIC;
	hdr.padding0 = 0;
	hdr.count = count;

	array = (struct vast_addr_t*)malloc(count * sizeof(struct vast_addr_t));

	offset = sizeof(struct vast_hdr_t) +		// header
		sizeof(struct vast_addr_t) * count;	// addr array

	for (count = 0; symbols[count].name; count++)
	{
		array[count].virt_addr = symbols[count].virt_addr;
		array[count].name_offset = offset;
		offset += 1 + strlen(symbols[count].name);
	}

	fwrite(&hdr, 1, sizeof(hdr), fp);
	fwrite(array, count, sizeof(array[0]), fp);

	for (count = 0; symbols[count].name; count++)
	{
		fputs(symbols[count].name, fp);
		fputc(0, fp);
	}

	fclose(fp);
	free(array);

	return 0;
}
