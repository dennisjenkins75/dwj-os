/*	tools/core-debugger/core-debugger.h

	Utility that runs on linux host running the OS in vmware.
	Given filename of core file and value of CR3, will dump
	mapped virtual memory.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

// need 'basename'
#include <libgen.h>

typedef unsigned int uint32;
typedef unsigned short int uint16;
typedef unsigned char uint8;

#include "kernel/kernel/config.h"
#include "kernel/kernel/corehelp.h"

struct  symbol_t
{
	uint32          virt_addr;
	const char      *name;
};

// Pointer to loaded corehelp struct.
extern struct corehelp *corehelp;

extern const char*	ResolveSymbol(uint32 virtaddr);

extern uint8	ReadByte(uint32 virtaddr);

extern uint32	ReadDword(uint32 virtaddr);

extern char*	DupString(uint32 virtaddr);

extern void	DumpMemory(uint32 offset, uint32 count);

extern void	DumpHeap(void);

extern void	DumpStack(uint32 ebp);

extern uint32 parse_value(const char *str);

extern void	LoadCore(const char *filename);
