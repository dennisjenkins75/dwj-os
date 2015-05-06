/*	tools/core-debugger/stack.c	*/

#include "core-debugger.h"

/*
	Shamelessly stolen from MIT: http://pdos.csail.mit.edu/6.828/2004/lec/l2.html

		       +------------+   |
		       | arg 2      |   \
		       +------------+    >- previous function's stack frame
		       | arg 1      |   /
		       +------------+   |
		       | ret %eip   |   /
		       +============+
		       | saved %ebp |   \
		%ebp-> +------------+   |
		       |            |   |
		       |   local    |   \
		       | variables, |    >- current function's stack frame
		       |    etc.    |   /
		       |            |   |
		       |            |   |
		%esp-> +------------+   /
*/

void	DumpStack(uint32 ebp)
{
	uint32	next_frame = 0;
	uint32	this_frame = ebp;
	uint32	ret_addr = 0;
	const char *symbol = NULL;

	DumpMemory(ebp - 32, 128);

	printf("--------   Dump Stack (ebp = %08x)\n", ebp);

	for (;;)
	{
		next_frame = ReadDword(this_frame);
		ret_addr = ReadDword(this_frame + 4);
		symbol = ResolveSymbol(ret_addr);

		printf("frame: %08x   ret_addr: %08x  (%s)\n", this_frame, ret_addr, symbol);

		if (this_frame >= next_frame)
		{
			break;
		}

		this_frame = next_frame;
	}
}
