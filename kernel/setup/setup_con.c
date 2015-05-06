/*	kernel/setup_con.c

	Implements a very rudimentary console driver for use by the
	".setup.text" section.  The normal console is in high-memory
	and is accessed from the .text section.
*/

#include "kernel/kernel.h"

// Supports a very small 'printf' console printer for debugging the .setup.text code.
static uint16 *_bios_vga_base ATTR_SETUP_DATA = (uint16*)0xb8000;
uint16 *_setup_vga_base ATTR_SETUP_DATA = (uint16*)0xb8000;
static uint32 vga_x ATTR_SETUP_DATA = 0;
static uint32 vga_y ATTR_SETUP_DATA = 0;

// Hack allows us to find string literals in ".data" before we enable paging.
// _setup_vga_base is altered immediately after paging is turned on.
static const char * ATTR_SETUP_TEXT HACK_STRING(const char *str)
{
	if ((_bios_vga_base == _setup_vga_base) &&
	    ((uint32)str >= (uint32)&_kernel_virtual))
	{
		str -= (uint32)&_kernel_virtual;
	}
	return str;
}

void ATTR_SETUP_TEXT _setup_outportb(uint16 _port, uint8 _data)
{
	__asm__ __volatile__ ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

#if (DEBUG_LOG_TO_LPT)
static void ATTR_SETUP_TEXT write_lpt(char ch)
{
	_setup_outportb(LPT_BASE, ch);
	_setup_outportb(LPT_BASE + 2, 0x0c);
	_setup_outportb(LPT_BASE + 2, 0x0d);
}
#endif

#if (DEBUG_LOG_BOCHS_E9)
static void ATTR_SETUP_TEXT write_bochs(char ch)
{
	_setup_outportb(0xe9, ch);
}
#endif

void ATTR_SETUP_TEXT _setup_con_get_cursor(uint32 *x, uint32 *y)
{
	*x = vga_x;
	*y = vga_y;
}

static void ATTR_SETUP_TEXT _clrscr(void)
{
	uint16 *vga = _setup_vga_base;
	uint16 *end = _setup_vga_base + 80 * 25;

	vga_x = vga_y = 0;
	for (; vga < end; *vga = 0, vga++);
}

static void ATTR_SETUP_TEXT _scroll(void)
{
	uint16 *src = _setup_vga_base + 80;
	uint16 *dest = _setup_vga_base;
	int i;

	for (i = 0; i < 24 * 80; i++, src++, dest++)
	{
		*dest = *src;
	}

	for (i = 0; i < 80; i++, dest++)
	{
		*dest = 0;
	}
}

uint16*	ATTR_SETUP_TEXT _setup_calc_vram_cursor(void)
{
	return _setup_vga_base + (vga_y * 80 + vga_x);
}

void ATTR_SETUP_TEXT _setup_putchar(char ch)
{
#if (DEBUG_LOG_TO_LPT)
	write_lpt(ch);
#endif

#if (DEBUG_LOG_BOCHS_E9)
	write_bochs(ch);
#endif

	switch (ch)
	{
		case '\r':
			vga_x = 0;
			break;

		case '\n':
			for (; vga_x < 80; _setup_vga_base[vga_y * 80 + vga_x] = 0, vga_x++);
			vga_x = 0;
			vga_y++;
			break;

		case '\t':
			vga_x = (vga_x & 0xf8) + 8;
			break;

		default:
			if (ch >= ' ')
			{
				_setup_vga_base[vga_y * 80 + vga_x] = 0x0700 | ch;
			}
			else
			{
				_setup_vga_base[vga_y * 80 + vga_x] = 0x1f00 | ch;
			}
			vga_x++;
			break;
	}

	if (vga_x > 79)
	{
		vga_x = 0;
		vga_y++;
	}

	if (vga_y > 24)
	{
		vga_y = 24;
		_scroll();
	}
}

// Note: this is not a conforming, nor bug-free printf.  It is a temp
// function used for debugging.  It WILL crash if given a bad format
// string.  It only understands a limited set of format specifiers.
static int ATTR_SETUP_TEXT _do_printf(const char *fmt, va_list args)
{
	int count = 0;
	int radix = 0;
	int fill = 0;
	const char *str = NULL;

	for (; *fmt; fmt++)
	{
		if (*fmt != '%')
		{
			count++;
			_setup_putchar(*fmt);
			continue;
		}

// Must be a format specifier.
// NOTE: I determined via trial and error that if the following "switch" statement has more than 8
// cases that it will cause my kernel to triple fault.  No clue why.

		fmt++;
		switch (*fmt)
		{
			case '%':
				count++;
				_setup_putchar(*fmt);
				break;

			case 'p':
				fill = 8;
				radix = 16;
				break;

			case 'x':
				radix = 16;
				break;

			case 'd':
			case 'u':
				radix = 10;
				break;

// Eliminating octal and binary so that I can have 'c'.
//			case 'o':
//				radix = 8;
//				break;

//			case 'b':
//				fill = 8;
//				radix = 2;
//				break;

			case 's':
				str = va_arg(args, const char *);
				str = HACK_STRING(str);
				for (; *str; _setup_putchar(*str), str++, count++);
				break;

			case 'c':
				_setup_putchar((char)va_arg(args, unsigned long));
				count++;
				break;
		}

		if (radix)
		{
			uint32 value = va_arg(args, unsigned long);
			char temp_str[81];
			char *where = temp_str + sizeof(temp_str) - 1;

			*(where) = 0;	// terminate string.

			do
			{
				uint32 temp = value % radix;

				where--;
				fill--;
				if (temp < 10) *where = temp + '0';
				else *where = temp - 10 + 'a';

				value /= radix;
			}
			while (value);

			while (fill > 0)
			{
				where--;
				*where = '0';
				fill--;
			}

			while (*where)
			{
				count++;
				_setup_putchar(*where);
				where++;
			}

			radix = 0;
			fill = 0;
		}
	}

	return count;
}

int ATTR_SETUP_TEXT _setup_log(const char *fmt, ...)
{
	int i;
	va_list	va;

	fmt = HACK_STRING(fmt);

	va_start(va, fmt);
	i = _do_printf(fmt, va);
	va_end(va);

	return i;
}


// FIXME: This code successfully calls the vmware hypervisor, but
// it does not cause the cursor to "ungrab" as expected.
/*
This code causes the following to appear in the vmware log.

Jul 22 01:52:05: vcpu-0| TOOLS received request in VMX to set option 'synctime' -> '0'
Jul 22 01:52:05: vcpu-0| TOOLS received request in VMX to set option 'copypaste' -> '1'
*/

/*
static void	ATTR_SETUP_TEXT disable_vmware_mouse_capture(void)
{
	struct vmw_regs	regs;

// "vmware_invoke()" is linked in the .text section.  But that is not
// mapped yet.  So we need to compute it's physical address so that we
// can call it using our current non-paged memory identity map.
// NOTE: This is only safe b/c "vmware_invoke()" does not call
// and other functions.
	void (*vmware)(struct vmw_regs *regs) =
		(void (*)(struct vmw_regs *))
		((uint8*)&vmware_invoke - (uint8*)&_kernel_virtual);

Halt(); // the above code will fail once paging is turned on.

//	_setup_log("vmware = %p, %p\n", vmware, &vmware_invoke);

	regs.eax = VMWARE_MAGIC;
	regs.ebx = 0;
	regs.ecx = 0x0d;		// Get GUI options.
	regs.edx = VMWARE_PORT;
	regs.esi = 0;
	regs.edi = 0;
	regs.ebp = 0;

	vmware(&regs);			// vmware_invoke(&regs);

	regs.ebx = regs.eax | 0x02;	// set "ungrab when cursor leaves."
	regs.eax = VMWARE_MAGIC;
	regs.ecx = 0x0e;		// Set GUI options.
	regs.edx = VMWARE_PORT;

	vmware(&regs);			// vmware_invoke(&regs);

//	_setup_log("cursor ungrabbed!\n");
}
*/
static const char ATTR_SETUP_DATA *str = "DWJOS booting...\n";

void ATTR_SETUP_TEXT _setup_init_console(void)
{
#if (DEBUG_LOG_TO_LPT)
	int i;
	for (i = 0; i < 80; i++) write_lpt('-');
	write_lpt('\r');
	write_lpt('\n');
#endif

	_clrscr();
//	disable_vmware_mouse_capture();

	_setup_log(str);
}
