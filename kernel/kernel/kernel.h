/* kernel/kernel.h */

#ifndef	__KERNEL_H__
#define	__KERNEL_H__

#define ATTR_SETUP_TEXT		__attribute__((__section__(".setup.text")))
#define ATTR_SETUP_RODATA	__attribute__((__section__(".setup.rodata")))
#define ATTR_SETUP_DATA		__attribute__((__section__(".setup.data")))
#define ATTR_SETUP_BSS		__attribute__((__section__(".setup.bss")))

#define ATTR_KERNEL_TEXT	__attribute__((__section__(".text")))
#define ATTR_KERNEL_RODATA	__attribute__((__section__(".rodata")))
#define ATTR_KERNEL_DATA	__attribute__((__section__(".data")))
#define ATTR_KERNEL_BSS		__attribute__((__section__(".bss")))

// http://gcc.gnu.org/onlinedocs/gcc/Offsetof.html
#define offsetof(type, member)  __builtin_offsetof (type, member)

#define MAX_INT (0x7fffffff)
#define MIN_INT (0x80000000)

#define NULL ((void*)0)
typedef	int size_t;
typedef int ssize_t;
typedef signed long long off64_t;
typedef unsigned long long uint64;
typedef signed long long int64;
typedef unsigned long uint32;
typedef signed long int32;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned char uint8;
typedef signed char int8;
typedef signed long taskid_t;

struct _handle { void * _unused; };
typedef struct _handle* handle;

#define countof(x) (sizeof(x) / sizeof((x)[0]))
#define max(x,y) ((x) > (y) ? (x) : (y))
#define min(x,y) ((x) < (y) ? (x) : (y))

#include "kernel/kernel/config.h"
#include "kernel/arch/i386.h"
#include "kernel/arch/intr.h"
#include "kernel/lib/errno.h"
#include "kernel/lib/stdarg.h"
#include "kernel/lib/assert.h"
#include "kernel/lib/printf.h"
#include "kernel/lib/lib.h"
#include "kernel/kernel/spinlock.h"
#include "kernel/kernel/debug.h"
#include "kernel/kernel/multiboot.h"
#include "kernel/kernel/task.h"
#include "kernel/kernel/objects.h"
#include "kernel/kernel/vast.h"
#include "kernel/kernel/corehelp.h"
#include "kernel/ktasks/ktasks.h"
#include "kernel/vmm/vmm.h"
#include "kernel/vmm/heap.h"
#include "kernel/fs/vfs.h"
#include "kernel/fs/devfs.h"
#include "kernel/drivers/vmware.h"
#include "kernel/drivers/console.h"
#include "kernel/drivers/pci.h"
#include "kernel/drivers/ata.h"

/* breakpoint.c */
extern void set_breakpoint(void *addr);
extern void clear_breakpoint(void *addr);

/* main.c */
extern char g_kcmdline[256];
extern int kmain(const struct phys_multiboot_info *mbi);

/* gdt.c */
extern void gdt_install(void);

/* idt.c */
extern void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags);
extern void idt_install();

/* timer.c */
extern uint32 timer_getcount(void);
extern uint64 time_get_abs(void);
extern void set_timer_phase(int hz);
extern void timer_wait(int ticks);
extern void timer_install();

/* keyboard.c */
extern void keyboard_install();

// panic.c
extern void dump_stack(uint32 ebp);
extern void panic(const char *file, const char *func, int line, const char *fmt, ...);

/* reboot.c */
extern void hard_reboot(void);

/* setup_con.c */
extern void ATTR_SETUP_TEXT _setup_con_get_cursor(uint32 *x, uint32 *y);
extern uint16* ATTR_SETUP_TEXT _setup_calc_vram_cursor(void);
extern void ATTR_SETUP_TEXT _setup_putchar(char ch);
extern int ATTR_SETUP_TEXT _setup_log(const char *fmt, ...); // __attribute__ ((format (printf, 1, 2)));
extern void ATTR_SETUP_TEXT _setup_init_console(void);


#endif	// __KERNEL_H__
