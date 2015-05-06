/*	tools/core-debugger/main.c

	Utility that runs on linux host running the OS in vmware.
	Given filename of core file and value of CR3, will dump
	mapped virtual memory.
*/

#include "core-debugger.h"

void	DumpMemory(uint32 offset, uint32 count)
{
	uint32	start = offset & 0xfffffff0;
	uint32	end = (offset + count + 15) & 0xfffffff0;
	int	i;
	uint8	ch;

	printf("--------   DumpMemory(%08x, %x)\n", offset, count);

	while (start < end)
	{
		printf("%08x: ", start);

		for (i = 0; i < 16; i++)
		{
			printf(" %02x", ReadByte(start + i));
			if (i == 7) printf (" ");
		}

		printf(" ");
		for (i = 0; i < 16; i++)
		{
			ch = ReadByte(start + i);
			isprint(ch) ? putc(ch, stdout) : putc('.', stdout);
			if (i == 7) printf (" ");
		}

		printf("\n");
		start += 16;
	}
}

// Converts string from hex ('0x12345678') or decimal ('1234')
// into an unsigned integer.
uint32	parse_value(const char *str)
{
	uint32 result = 0;

	if ((str[0] == '0') && (str[1] == 'x'))
	{
		sscanf(str, "0x%x", &result);
		return result;
	}

	return atoi(str);
}

int	main(int argc, char *argv[])
{
	char		*core_file = NULL;
	uint32		stack = 0;
	uint32		heap = 0;
	int		i;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-core"))
		{
			core_file = argv[++i];
		}
		else if (!strcmp(argv[i], "stack"))
		{
			stack = parse_value(argv[++i]);
		}
		else if (!strcmp(argv[i], "heap"))
		{
			heap = 1;
		}
	}

	if (!core_file)
	{
		fprintf(stderr, "Must specify '-core filename'.\n");
		exit(-1);
	}

	LoadCore(core_file);

	if (stack)
	{
		DumpStack(stack);
	}

	if (heap)
	{
		DumpHeap();
	}

	return 0;
}
