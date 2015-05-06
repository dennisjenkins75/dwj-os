/* kernel/console.h */

// http://my.execpc.com/CE/AC/geezer/osd/graphics/modes.c
// http://www.osdever.net/FreeVGA/home.htm
// http://www.osdever.net/FreeVGA/vga/vgafx.htm

#ifndef	__CONSOLE_H__
#define	__CONSOLE_H__

// VGA mode values (vga_setmode(x)):
#define VGA_MODE_80x25			0
#define VGA_MODE_80x50			1
#define VGA_MODE_90x30			2
#define VGA_MODE_90x60			3

// VGA IO Ports
#define VGA_AC_INDEX			0x3c0
#define VGA_AC_WRITE			0x3c0
#define VGA_AC_READ			0x3c1
#define VGA_MISC_WRITE			0x3c2
#define VGA_SEQ_INDEX			0x3c4
#define VGA_SEQ_DATA			0x3c5
#define VGA_DAC_READ_INDEX		0x3c7
#define VGA_DAC_WRITE_INDEX		0x3c8
#define VGA_DAC_DATA			0x3c9
#define VGA_MISC_READ			0x3cc
#define VGA_GC_INDEX			0x3ce
#define VGA_GC_DATA			0x3cf
#define VGA_CRTC_INDEX			0x3d4	/* 0x3b4 for mono */
#define VGA_CRTC_DATA			0x3d5	/* 0x3b5 for mono */
#define VGA_INSTAT_READ			0x3da

#define VGA_NUM_SEQ_REGS		5
#define VGA_NUM_CRTC_REGS		25
#define VGA_NUM_GC_REGS			9
#define VGA_NUM_AC_REGS			21
#define VGA_NUM_REGS			( 1 + VGA_NUM_SEQ_REGS + \
					VGA_NUM_CRTC_REGS + \
					VGA_NUM_GC_REGS + \
					VGA_NUM_AC_REGS )


// VGA Registers.
#define VGA_REG_HORZ_TOTAL 		0x00
#define VGA_REG_HORZ_END_DISP		0x01
#define VGA_REG_HORZ_BLANK_START	0x02
#define VGA_REG_HORZ_BLANK_END		0x03
#define VGA_REG_HORZ_START_RETRACE	0x04
#define VGA_REG_HORZ_END_RETRACE	0x05
#define VGA_REG_VERT_TOTAL		0x06
#define VGA_REG_OVERFLOW		0x07
#define VGA_REG_PRESENT_ROW_SCAN	0x08
#define VGA_REG_MAX_SCAN_LINE		0x09
#define VGA_REG_CURSOR_START		0x0a
#define VGA_REG_CURSOR_END		0x0b
#define VGA_REG_START_ADDR_HIGH		0x0c
#define VGA_REG_START_ADDR_LOW		0x0d
#define VGA_REG_CURSOR_LOC_HIGH		0x0e
#define VGA_REG_CURSOR_LOC_LOW		0x0f
#define VGA_REG_VERT_RETRACE_START	0x10
#define VGA_REG_VERT_RETRACE_END	0x11
#define VGA_REG_VERT_DISPLAY_END	0x12
#define VGA_REG_OFFSET			0x13
#define VGA_REG_UNDERLINE_LOC		0x14
#define VGA_REG_VERT_BLANK_START	0x15
#define VGA_REG_VERT_BLANK_END		0x16
#define VGA_REG_CRTC_MODE_CTRL		0x17
#define VGA_REG_LINE_COMPARE		0x18

#define VGA_REGISTER_COUNT		0x19

#define VGA_PORT			0x3d4

extern unsigned char g_40x25_text[];
extern unsigned char g_40x50_text[];
extern unsigned char g_80x25_text[];
extern unsigned char g_80x50_text[];
extern unsigned char g_90x30_text[];
extern unsigned char g_90x60_text[];
extern unsigned char g_640x480x2[];
extern unsigned char g_320x200x4[];
extern unsigned char g_640x480x16[];
extern unsigned char g_720x480x16[];
extern unsigned char g_320x200x256[];
extern unsigned char g_320x200x256_modex[];
extern unsigned char g_8x8_font[2048];
extern unsigned char g_8x16_font[4096];


extern void con_init(int video_mode);
extern uint8 con_settextcolor(unsigned char forecolor, unsigned char backcolor);
extern uint8 con_set_attr(uint8 attr);
extern void con_puts(const char *text);
extern void con_putch(unsigned char c);
extern void con_cls();
extern void con_xy_clear(int x, int y, int w, int h);
extern void con_print(int x, int y, unsigned char attrib, size_t len, const char *str);

extern void con_set_window(int line);

#endif	// __CONSOLE_H__
