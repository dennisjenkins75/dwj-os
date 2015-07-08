/*	kernel/ata.c

	http://www.ata-atapi.com/
*/

#include "kernel/kernel.h"

static volatile int	irq_received = 0;

void	ata_irq_handler(struct regs *r)
{
	kdebug (DEBUG_DEBUG, FAC_DISK_ATA, "ata: irq received.\n");
	irq_received = 1;
}

int	ata_read_lba28(uint16 base_port, int ms, uint32 lba28, uint8 count, void *buffer)
{
	uint8	v;

	if ((ms > 1) || (lba28 > ((1<<28)-1)) || !buffer || !count)
	{
		return -EINVAL;
	}

	outportb(base_port + ATA_IO_SECTOR_COUNT, count);
	Nop();

	outportb(base_port + ATA_IO_LBA_0_7, lba28 & 0xff);
	Nop();

	outportb(base_port + ATA_IO_LBA_8_15, (lba28 >> 8) & 0xff);
	Nop();

	outportb(base_port + ATA_IO_LBA_16_23, (lba28 >> 16) & 0xff);
	Nop();

	v = (lba28 >> 24) & 0x0f;	// lower nibble = high end of lba bits.
	if (ms) v |= 0x10;		// select slave drive.
	v |= 0x40;			// use LBA mode.
	outportb(base_port + ATA_IO_LBA_24_27, v);
	Nop();

	irq_received = 0;
	outportb(base_port + ATA_IO_COMMAND, ATA_CMD_READ_SECTORS);
	Nop();

	// Wait for DRQ to be set and BUSY to be clear.

FIXME(); // need a time limit or something.
	do
	{
		v = inportb(base_port + ATA_IO_STATUS);

		v &= (ATA_STATUS_BUSY | ATA_STATUS_DRQ);

		if (v == ATA_STATUS_DRQ)
		{
			break;
		}

		kdebug (DEBUG_DEBUG, FAC_DISK_ATA, ">");
	} while (1);


	// then wait for interrupt.
	kdebug (DEBUG_DEBUG, FAC_DISK_ATA, "ata: read: waiting for interrupt.\n");
	while (!irq_received)
	{
		kdebug (DEBUG_DEBUG, FAC_DISK_ATA, ".");
	}

	// get the status again. (required by the spec?)
	v = inportb(base_port + ATA_IO_STATUS);
	Nop();

FIXME(); // Clean up the ASM below

	{
		uint32 word_count = count * 256;
		uint32 io_port = base_port + ATA_IO_DATA;

		// read from data port into our buffer.
		__asm__ __volatile__
		(
			"movl	%0, %%edx\n"	// Base IO port.
			"movl	%1, %%ecx\n"	// Word count.
			"movl	%2, %%edi\n"	// dest buffer.
			"cld\n"
			"repne\n"
			"insw\n"
			: // outputs
			: "m" (io_port), "m" (word_count), "m"(buffer) // inputs
			: "%eax", "%ecx", "%edx", "%edi" // clobbers
		);
	}

	kdebug_mem_dump (DEBUG_DEBUG, FAC_DISK_ATA, buffer, 512);

	return 0;
}

int	ata_probe_1(uint16 base_port, uint8 value)
{
	outportb(base_port + ATA_IO_LBA_0_7, value);
	Nop();	// wait min 1 bus cycle.
	return value == inportb(base_port + ATA_IO_LBA_0_7);
}

void	ata_probe_ctrlr(uint16 base_port)
{
// Write a random value to the LBA bits 0-7 port,
// wait, and try to read it back.

	if (!ata_probe_1(base_port, 0x5a) ||
	    !ata_probe_1(base_port, 0xa5))
	{
		return;
	}

	kdebug (DEBUG_DEBUG, FAC_DISK_ATA, "ata: controller found at %04x\n", base_port);

// Probe for master device on controller.
	outportb(base_port + ATA_IO_DRIVE_HEAD, 0xa0);
	Nop();
	if (ATA_STATUS_READY & inportb(base_port + ATA_IO_STATUS))
	{
		kdebug (DEBUG_DEBUG, FAC_DISK_ATA, "ata: found primary device\n");
	}

	outportb(base_port + ATA_IO_DRIVE_HEAD, 0xa0 | 0x10);
	Nop();
	if (ATA_STATUS_READY & inportb(base_port + ATA_IO_STATUS))
	{
		kdebug (DEBUG_DEBUG, FAC_DISK_ATA, "ata: found secondary device\n");
	}
}


void	ata_test(void)
{
	ata_probe_ctrlr(ATA_CTRLR_0_BASE);
	ata_probe_ctrlr(ATA_CTRLR_1_BASE);

	irq_set_handler(14, ata_irq_handler);
	irq_set_handler(15, ata_irq_handler);

	{
		void *buffer = kmalloc(512 * 8, 0);
		int r = ata_read_lba28(ATA_CTRLR_0_BASE, 0, 0, 1, buffer);
//int	ata_read_lba28(uint16 base_port, int ms, uint32 lba28, uint8 count, void *buffer)

		kdebug (DEBUG_DEBUG, FAC_DISK_ATA, "ata: read = %d\n", r);

		kfree(buffer);
	}
}
