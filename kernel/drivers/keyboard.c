/* bkerndev - Bran's Kernel Development Tutorial
*  By:   Brandon F. (friesenb@gmail.com)
*  Desc: Keyboard driver
*
*  Notes: No warranty expressed or implied. Use at own risk. */
#include "kernel/kernel.h"

// Ring buffer to hold raw scan codes.  Insert at head, remove at
// tail, drop chars if buffer is full.
static uint8	kbd_buffer[KBD_BUFFER_SIZE];
static int	buf_head = 0;
static int	buf_tail = 0;

// Current keyboard state (bit flags)
//static uint16	kbd_flags = 0;

// Synchronize access between ISR and higher level functions.
// Used to guard access to ring-buffer and flags.
static spinlock kbd_lock = INIT_SPINLOCK("kbd");

/* KBDUS means US Keyboard Layout. This is a scancode table
*  used to layout a standard US keyboard. I have left some
*  comments in to give you an idea of what key is what, even
*  though I set it's array index to 0. You can change that to
*  whatever you want using a macro, if you wish! */
unsigned char kbdus[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',		/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,					/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

/* Handles the keyboard interrupt */
void	keyboard_handler(struct regs *r)
{
	uint8	scancode = inportb(0x60);

	spinlock_acquire(&kbd_lock);

// Do we have room for the scan code?
	if (((buf_head + 1) % KBD_BUFFER_SIZE) == buf_tail)
	{
		printf("kbd: buffer full, dropping scan code %02x\n", scancode);
	}
	else
	{
		kbd_buffer[buf_head] = scancode;
		buf_head = (buf_head + 1) % KBD_BUFFER_SIZE;
	}

	if (scancode == 0x58)	// F12 key
	{
		task_dump_list();
	}

	spinlock_release(&kbd_lock);

	// FIXME: Need to signal a DPC

	if (scancode == 0x46)	// seen during CTRL-BREAK.
	{
		hard_reboot();
	}
}

// Returns next scan code from right buffer, or 0xff if none in buffer.
uint8	keyboard_next_code(void)
{
	uint8	ret = 0xff;

	spinlock_acquire(&kbd_lock);

	if (buf_head != buf_tail)
	{
		ret = kbd_buffer[buf_tail];
		buf_tail = (buf_tail + 1) % KBD_BUFFER_SIZE;
	}

	spinlock_release(&kbd_lock);

	return ret;
}




/*

//   If the top bit of the byte we read from the keyboard is
//      set, that means that a key has just been released.
    if (scancode & 0x80)
    {
        // You can use this one to see if the user released the
        //  shift, alt, or control keys...
    }
    else
    {
        // Here, a key was just pressed. Please note that if you
        //  hold a key down, you will get repeated key press
        //  interrupts.

        // Just to show you how this works, we simply translate
        //  the keyboard scancode into an ASCII value, and then
        //  display it to the screen. You can get creative and
        //  use some flags to see if a shift is pressed and use a
        //  different layout, or you can add another 128 entries
        //  to the above layout to correspond to 'shift' being
        //  held. If shift is held using the larger lookup table,
        //  you would add 128 to the scancode when you look for it
//        putch(kbdus[scancode]);

//	printf("Keyboard: code = 0x%02x, char = '%c'.\n", scancode, kbdus[scancode]);

	{
		char temp[64];
		char ch = kbdus[scancode];
		int len = sprintf(temp, "key: %02x '%c'", scancode, ch ? ch : 0xff);
		con_print(80 - len, 1, 0x5f, len, temp);

		switch (ch)
		{
			case '+':
				*(int*)0xeffffffc = 0;	// force crash.
				break;

			case '\n':
				printf("\rclock_getcount() =  %u\n", timer_getcount());
				break;
		}
	}


	if (scancode == 0x46)	// seen during CTRL-BREAK.
	{
		hard_reboot();
	}
    }
*/



/* Installs the keyboard handler into IRQ1 */
void	keyboard_install(void)
{
    irq_set_handler(1, keyboard_handler);
}
