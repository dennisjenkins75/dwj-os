/*	kernel/kernel/debug.c	*/

#include "kernel/kernel.h"

int	kdebug_outch (unsigned char ch, void **ptr)
{
#if (DEBUG_LOG_TO_LPT)
	outportb(LPT_BASE, ch);
	outportb(LPT_BASE + 2, 0x0c);
	outportb(LPT_BASE + 2, 0x0d);
#endif

#if (DEBUG_LOG_BOCHS_E9)
	outportb(0xe9, ch);
#endif

	return 0;
}

void	kdebug (int nLevel, int nFacility, const char *fmt, ...)
{
	va_list		va;

	va_start (va, fmt);
	do_printf (fmt, -1, va, kdebug_outch, (void*)NULL);
	va_end (va);
}

void	kdebug_mem_dump (int nLevel, int nFacility, const void *addr, uint32 bytes)
{
	uint8*	start = (uint8*)((uint32)addr & ~0xf);			// round down to nearest 16
	uint8*	end = (uint8*)(((uint32)addr + bytes + 0x0f) & ~0xf);	// round up to nearest 16
	uint32	i;

	while (start < end)
	{
		char buffer[256];
		char *p = buffer;
		char *e = buffer + sizeof(buffer);

		p += snprintf (p, e - p, "mem_dump [%p]: ", start);

		for (i = 0; i < 16; i++)
		{
			p += snprintf(p, e - p, " %02x", start[i]);
			if (i == 7) snprintf (p, e - p, " ");
		}
		p += snprintf(p, e - p, "  ");

		for (i = 0; i < 16; i++)
		{
			p += snprintf(p, e - p, "%c", start[i] >= ' ' ? start[i] : '.');
			if (i == 7) snprintf (p, e - p, " ");
		}
		p += snprintf(p, e - p, "\n");
		kdebug (nLevel, nFacility, "%s", buffer);

		start += 16;
	}
}
