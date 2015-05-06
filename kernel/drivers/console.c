/*	kernel/console.c	*/

#include "kernel/kernel.h"

// Number of bytes available in text console.
#define VGA_MEM_SIZE 32768

// virtual memory address of entire TEXT frame buffer.  Does not change.
static uint16	*text_abs = (uint16*)&_kernel_console_start;

// Pointer to fist visible character in then frame buffer.  Changes as
// the screen scrolls.  Will wrap back to 'text_abs'.
static uint16	*text_cur = (uint16*)&_kernel_console_start;

// Max number of lines in frame buffer.
static uint32	max_lines = VGA_MEM_SIZE / 25;

static uint8	attrib = 0x07;
static uint32	csr_x = 0;
static uint32	csr_y = 0;
static uint16	vga_port = VGA_PORT;
static uint32	con_cols = 80;
static uint32	con_rows = 25;

// We can't lock the console if we are debugging spinlocks (system will dead-lock).
#if !(DEBUG_INT_DISABLE)
#define LOCK_CONSOLE 1
#endif

#if defined (LOCK_CONSOLE)
static spinlock	console_lock = INIT_SPINLOCK("console");
#endif

struct	vga_mode_t
{
	uint32		width;
	uint32		height;
	unsigned char	*regs;
	unsigned char	*font;
	uint32		font_height;
};

static struct vga_mode_t vga_mode_list[] =
{
	{ 80, 25, g_80x25_text, g_8x16_font, 16 },
	{ 80, 50, g_80x50_text, g_8x8_font, 8 },
	{ 90, 30, g_90x30_text, g_8x16_font, 16 },
	{ 90, 60, g_90x60_text, g_8x8_font, 8 }
};

static const int vga_mode_count = (sizeof(vga_mode_list) / sizeof(vga_mode_list[0]));


static void update_hardware_cursor(void)
{
	unsigned temp = (text_cur - text_abs) + csr_x;

	outportb(vga_port, VGA_REG_CURSOR_LOC_HIGH);
	outportb(vga_port+1, temp >> 8);
	outportb(vga_port, VGA_REG_CURSOR_LOC_LOW);
	outportb(vga_port+1, temp);
}

void	vga_read_regs(unsigned char *regs)
{
	unsigned i;

/* read MISCELLANEOUS reg */
	*regs = inportb(VGA_MISC_READ);
	regs++;
/* read SEQUENCER regs */
	for(i = 0; i < VGA_NUM_SEQ_REGS; i++)
	{
		outportb(VGA_SEQ_INDEX, i);
		*regs = inportb(VGA_SEQ_DATA);
		regs++;
	}
/* read CRTC regs */
	for(i = 0; i < VGA_NUM_CRTC_REGS; i++)
	{
		outportb(VGA_CRTC_INDEX, i);
		*regs = inportb(VGA_CRTC_DATA);
		regs++;
	}
/* read GRAPHICS CONTROLLER regs */
	for(i = 0; i < VGA_NUM_GC_REGS; i++)
	{
		outportb(VGA_GC_INDEX, i);
		*regs = inportb(VGA_GC_DATA);
		regs++;
	}
/* read ATTRIBUTE CONTROLLER regs */
	for(i = 0; i < VGA_NUM_AC_REGS; i++)
	{
		(void)inportb(VGA_INSTAT_READ);
		outportb(VGA_AC_INDEX, i);
		*regs = inportb(VGA_AC_READ);
		regs++;
	}
/* lock 16-color palette and unblank display */
	(void)inportb(VGA_INSTAT_READ);
	outportb(VGA_AC_INDEX, 0x20);
}

void	vga_write_regs(unsigned char *regs)
{
	unsigned i;

/* write MISCELLANEOUS reg */
	outportb(VGA_MISC_WRITE, *regs);
	regs++;
/* write SEQUENCER regs */
	for(i = 0; i < VGA_NUM_SEQ_REGS; i++)
	{
		outportb(VGA_SEQ_INDEX, i);
		outportb(VGA_SEQ_DATA, *regs);
		regs++;
	}
/* unlock CRTC registers */
	outportb(VGA_CRTC_INDEX, 0x03);
	outportb(VGA_CRTC_DATA, inportb(VGA_CRTC_DATA) | 0x80);
	outportb(VGA_CRTC_INDEX, 0x11);
	outportb(VGA_CRTC_DATA, inportb(VGA_CRTC_DATA) & ~0x80);
/* make sure they remain unlocked */
	regs[0x03] |= 0x80;
	regs[0x11] &= ~0x80;
/* write CRTC regs */
	for(i = 0; i < VGA_NUM_CRTC_REGS; i++)
	{
		outportb(VGA_CRTC_INDEX, i);
		outportb(VGA_CRTC_DATA, *regs);
		regs++;
	}
/* write GRAPHICS CONTROLLER regs */
	for(i = 0; i < VGA_NUM_GC_REGS; i++)
	{
		outportb(VGA_GC_INDEX, i);
		outportb(VGA_GC_DATA, *regs);
		regs++;
	}
/* write ATTRIBUTE CONTROLLER regs */
	for(i = 0; i < VGA_NUM_AC_REGS; i++)
	{
		(void)inportb(VGA_INSTAT_READ);
		outportb(VGA_AC_INDEX, i);
		outportb(VGA_AC_WRITE, *regs);
		regs++;
	}
/* lock 16-color palette and unblank display */
	(void)inportb(VGA_INSTAT_READ);
	outportb(VGA_AC_INDEX, 0x20);
}

/*****************************************************************************
VGA framebuffer is at A000:0000, B000:0000, or B800:0000
depending on bits in GC 6.  We map it to &_kernel_console_start.
*****************************************************************************/
static void* vga_get_fb_ptr(void)
{
	unsigned seg;

	outportb(VGA_GC_INDEX, 6);
	seg = inportb(VGA_GC_DATA);
	seg >>= 2;
	seg &= 3;
	switch(seg)
	{
		case 0:		// 0xa000
		case 1:		// 0xa000
			return (void*)&_kernel_console_start;
		case 2:		// 0xb000
			return (void*)((uint8*)&_kernel_console_start + 0x10000);
		case 3:		// 0xb800
			return (void*)((uint8*)&_kernel_console_start + 0x18000);
	}
	return NULL;	// should never happen.
}
/*****************************************************************************
*****************************************************************************/
static void vmemwr(unsigned dst_off, unsigned char *src, unsigned count)
{
//#define	_vmemwr(DS,DO,S,N)	memcpy((char *)((DS) * 16 + (DO)), S, N)
//		_vmemwr(get_fb_seg(), dst_off, src, count);

	uint8	*ptr = (uint8*)vga_get_fb_ptr() + dst_off;
	memcpy(ptr, src, count);
}

/*
static void vpokeb(unsigned off, unsigned val)
{
	pokeb(get_fb_seg(), off, val);
}

static unsigned vpeekb(unsigned off)
{
	return peekb(get_fb_seg(), off);
}
*/

static void set_plane(unsigned p)
{
	unsigned char pmask;

	p &= 3;
	pmask = 1 << p;
/* set read plane */
	outportb(VGA_GC_INDEX, 4);
	outportb(VGA_GC_DATA, p);
/* set write plane */
	outportb(VGA_SEQ_INDEX, 2);
	outportb(VGA_SEQ_DATA, pmask);
}

/*****************************************************************************
write font to plane P4 (assuming planes are named P1, P2, P4, P8)
*****************************************************************************/
static void vga_write_font(unsigned char *buf, unsigned font_height)
{
	unsigned char seq2, seq4, gc4, gc5, gc6;
	unsigned i;

/* save registers
set_plane() modifies GC 4 and SEQ 2, so save them as well */
	outportb(VGA_SEQ_INDEX, 2);
	seq2 = inportb(VGA_SEQ_DATA);

	outportb(VGA_SEQ_INDEX, 4);
	seq4 = inportb(VGA_SEQ_DATA);
/* turn off even-odd addressing (set flat addressing)
assume: chain-4 addressing already off */
	outportb(VGA_SEQ_DATA, seq4 | 0x04);

	outportb(VGA_GC_INDEX, 4);
	gc4 = inportb(VGA_GC_DATA);

	outportb(VGA_GC_INDEX, 5);
	gc5 = inportb(VGA_GC_DATA);
/* turn off even-odd addressing */
	outportb(VGA_GC_DATA, gc5 & ~0x10);

	outportb(VGA_GC_INDEX, 6);
	gc6 = inportb(VGA_GC_DATA);
/* turn off even-odd addressing */
	outportb(VGA_GC_DATA, gc6 & ~0x02);
/* write font to plane P4 */
	set_plane(2);
/* write font 0 */
	for(i = 0; i < 256; i++)
	{
		vmemwr(16384u * 0 + i * 32, buf, font_height);
		buf += font_height;
	}
#if 0
/* write font 1 */
	for(i = 0; i < 256; i++)
	{
		vmemwr(16384u * 1 + i * 32, buf, font_height);
		buf += font_height;
	}
#endif
/* restore registers */
	outportb(VGA_SEQ_INDEX, 2);
	outportb(VGA_SEQ_DATA, seq2);
	outportb(VGA_SEQ_INDEX, 4);
	outportb(VGA_SEQ_DATA, seq4);
	outportb(VGA_GC_INDEX, 4);
	outportb(VGA_GC_DATA, gc4);
	outportb(VGA_GC_INDEX, 5);
	outportb(VGA_GC_DATA, gc5);
	outportb(VGA_GC_INDEX, 6);
	outportb(VGA_GC_DATA, gc6);
}

int	con_setmode(int mode)
{
	if ((mode < 0) || (mode >= vga_mode_count))
	{
		return -EINVAL;
	}

#if defined (LOCK_CONSOLE)
	spinlock_acquire(&console_lock);
#endif

	vga_write_regs(vga_mode_list[mode].regs);
	vga_write_font(vga_mode_list[mode].font, vga_mode_list[mode].font_height);

	con_cols = vga_mode_list[mode].width;
	con_rows = vga_mode_list[mode].height;

	max_lines = VGA_MEM_SIZE / (con_cols * sizeof(uint16));
FIXME(); // Need to readjust text_cur for new mode.

	text_cur = text_abs = (uint16*)vga_get_fb_ptr();

//	printf("console: text_abs = %p\n", text_abs);

#if defined (LOCK_CONSOLE)
	spinlock_release(&console_lock);
#endif

	return 0;
}

void	con_init(int video_mode)
{
// The setup code already initiaized console so that it can print
// diagnostics there.  We don't want to erase the data already on the
// console.
	_setup_con_get_cursor(&csr_x, &csr_y);
	update_hardware_cursor();

	con_setmode(video_mode);
	printf("console: initialized.\n");
}

void	con_scroll(void)
{
	uint32	cur_scroll = 0;
	uint16	*new_line = 0;

	cur_scroll = (text_cur - text_abs) / con_cols;
	new_line = text_cur + (con_cols * con_rows);

// We can't scroll beyond the end of the fraem buffer.  The frame buffer
// will not wrap smoothly (32768 / 80 is not an integer).
	if ((cur_scroll + con_rows) >= max_lines)
	{
// Copy the tail end of the frame buffer back to the top.
		memcpydw(text_abs, text_cur, con_rows * con_cols / 2);

		text_cur = text_abs;
		cur_scroll = 0;

		new_line = text_cur + (con_cols * con_rows);
	}

// Clear section of VRAM about to be displayed.
	memsetw(new_line, attrib << 8 | (uint16)' ' , con_cols);

// Update accounting info.
	cur_scroll++;
	text_cur += con_cols;
        csr_y = con_rows - 1;

// Update VGA scroll position.
	outportb(VGA_CRTC_INDEX, VGA_REG_START_ADDR_HIGH);
	outportb(VGA_CRTC_DATA, (uint8)((cur_scroll * con_cols) >> 8));
	outportb(VGA_CRTC_INDEX, VGA_REG_START_ADDR_LOW);
	outportb(VGA_CRTC_DATA, (uint8)((cur_scroll * con_cols) & 0xff));
}

/* Scrolls the screen */
void	con_scroll_slow(void)
{
    unsigned blank, temp;

    /* A blank is defined as a space... we need to give it
    *  backcolor too */
    blank = 0x20 | (attrib << 8);

    /* Row 25 is the end, this means we need to scroll up */
    if (csr_y >= con_rows)
    {
        /* Move the current text chunk that makes up the screen
        *  back in the buffer by a line */
        temp = csr_y - con_rows + 1;
        memcpy (text_cur, text_cur + temp * con_cols, (con_rows - temp) * con_cols * 2);

        /* Finally, we set the chunk of memory that occupies
        *  the last line of text to our 'blank' character */
        memsetw (text_cur + (con_rows - temp) * con_cols, blank, con_cols);
        csr_y = con_rows - 1;
    }
}


/* Clears the screen */
void 	con_cls()
{
	unsigned blank;
	int i;

	blank = 0x20 | (attrib << 8);

	for (i = 0; i < con_rows; i++)
	{
		memsetw (text_cur + i * con_cols, blank, con_cols);
	}

	csr_x = 0;
	csr_y = 0;
	update_hardware_cursor();
}

/* Puts a single character on the screen */
void	con_putch(unsigned char c)
{
    unsigned short *where;
    unsigned att = attrib << 8;

#if (DEBUG_LOG_TO_LPT)
	outportb(LPT_BASE, c);
	outportb(LPT_BASE + 2, 0x0c);
	outportb(LPT_BASE + 2, 0x0d);
#endif

#if (DEBUG_LOG_BOCHS_E9)
	outportb(0xe9, c);
#endif

#if defined (LOCK_CONSOLE)
	spinlock_acquire(&console_lock);
#endif

    /* Handle a backspace, by moving the cursor back one space */
    if(c == 0x08)
    {
        if(csr_x != 0) csr_x--;
    }
    /* Handles a tab by incrementing the cursor's x, but only
    *  to a point that will make it divisible by 8 */
    else if(c == 0x09)
    {
	int tab_stop = (csr_x + 8) & ~(8 - 1);
	for (where = text_cur + (csr_y * con_cols + csr_x); csr_x != tab_stop; *where = (' ' | att), where++, csr_x++);
//        csr_x = (csr_x + 8) & ~(8 - 1);
    }
    /* Handles a 'Carriage Return', which simply brings the
    *  cursor back to the margin */
    else if(c == '\r')
    {
        csr_x = 0;
    }
    /* We handle our newlines the way DOS and the BIOS do: we
    *  treat it as if a 'CR' was also there, so we bring the
    *  cursor to the margin and we increment the 'y' value */
    else if(c == '\n')
    {
	for (where = text_cur + (csr_y * con_cols + csr_x); csr_x < con_cols; *where = (' ' | att), where++, csr_x++);
        csr_x = 0;
        csr_y++;
    }
    /* Any character greater than and including a space, is a
    *  printable character. The equation for finding the index
    *  in a linear chunk of memory can be represented by:
    *  Index = [(y * width) + x] */
    else if(c >= ' ')
    {
        where = text_cur + (csr_y * con_cols + csr_x);
        *where = c | att;	/* Character AND attributes: color */
        csr_x++;
    }

    /* If the cursor has reached the edge of the screen's width, we
    *  insert a new line in there */
    if (csr_x >= con_cols)
    {
        csr_x = 0;
        csr_y++;
    }

	if (csr_y >= con_rows)
	{
		con_scroll();
	}

        update_hardware_cursor();

#if defined (LOCK_CONSOLE)
	spinlock_release(&console_lock);
#endif
}

/* Uses the above routine to output a string... */
void	con_puts(const char *text)
{
	for (; *text; con_putch(*text), text++);
}

/* Sets the forecolor and backcolor that we will use */
uint8	con_settextcolor(unsigned char forecolor, unsigned char backcolor)
{
	uint8	old_attr = attrib;

	attrib = (backcolor << 4) | (forecolor & 0x0F);

	return old_attr;
}

uint8	con_set_attr(uint8 attr)
{
	uint8	old_attr = attrib;
	attrib = attr;
	return old_attr;
}

void	con_xy_clear(int x, int y, int w, int h)
{
	unsigned short *line = NULL;
	unsigned short *limit = text_cur + (con_cols * con_rows) - 1;

	while (h > 0)
	{
		line = text_cur + (y + h) * con_cols + x;

		if ((line >= text_cur) && (line <= limit))
		{
			if (line + w > limit)
			{
				line = limit;
			}

			memsetw(line, 0, w);
		}
	}
}

void	con_print(int x, int y, unsigned char attrib, size_t len, const char *str)
{
	unsigned char *ptr = (unsigned char*)text_cur + 2 * (y * con_cols + x);
	unsigned char *limit = (unsigned char*)text_cur + 2 * ((con_cols * con_rows) - 1);

	while ((ptr >= (unsigned char*)text_cur) && (ptr < limit) && (len > 0) && *str)
	{
		*(ptr++) = *str;
		*(ptr++) = attrib;

		str++;
		len--;
	}
}

/*  Sets the VGA start address.  This is the start of where teh VGA
    circitry starts rendering the screen from.  The screen may be
    80x25 or 80x50, but the buffer is 32K and will hold 80x409
    lines */

void	con_set_window(int line)
{
	outportb(VGA_CRTC_INDEX, VGA_REG_START_ADDR_HIGH);
	outportb(VGA_CRTC_DATA, (uint8)(line >> 8));

	outportb(VGA_CRTC_INDEX, VGA_REG_START_ADDR_LOW);
	outportb(VGA_CRTC_DATA, (uint8)(line & 0xff));
}
