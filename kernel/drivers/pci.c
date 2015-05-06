/*	kernel/pci.c

	Contains functions for enumerating the PCI bus.


PCI devices found in vmware:
	8086:7190 (rev 01)	- PCI Host Bridge (Intel 82443BX)
	8086:7191 (rev 01)	- PCI AGP Bridge (Intel 82443)
	8086:7110 (rev 08)	- ISA Bridge (Intel 82371AB - PIIX4)
	8086:7111 (rev 01)	- IDE interface (Intel 82371AB - PIIX4)
	8086:7113 (rev 08)	- ACPI Bridge (Intel 82371AB)
	15ad:0405 		- VMWare SVGA II
	1000:0030 (rev 01)	- LSI Logic 53c1030 PCI-X Fusion Ultra320 SCSI
	1022:2000 (rev 10)	- AMD 79c970 (PCnet32 Lance) ethernet device.
	104b:1040		- VMWare usb hub ??
	1274:1371		- VMWare audio device (sb16 ??)

PCI devices found in virtual pc:
	8086:7192
	8086:7110
	5333:8811
	1011:0009

PCI Documentation:
	http://www.osdev.org/wiki/PCI

*/

#include "kernel/kernel.h"

uint16	pci_config_read_word(uint16 bus, uint16 slot, uint16 func, uint16 offset)
{
	uint32	address;
	uint32	lbus = bus;
	uint32	lslot = slot;
	uint32	lfunc = func;
	uint16	tmp;

// Compute configuration address.
	address = (uint32)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((uint32)0x80000000));

// Do bus transaction.
	outportl(0xcf8, address);

	tmp = (uint16)((inportl(0xcfc) >> ((offset & 2) * 8)) & 0xffff);

	return tmp;
}

void	pci_enum_devices(void)
{
	uint16	bus = 0;
	uint16	slot = 0;
	uint16	function = 0;
	uint16	vendor = 0;
	uint16	device = 0;
	uint16	status = 0;
	uint16	subsys_id = 0;
	uint16	subsys_vendor = 0;

	for (bus = 0; bus < 4; bus++)
	{
		for (slot = 0; slot < 64; slot++)
		{
			for (function = 0; function < 8; function++)
			{
				vendor = pci_config_read_word(bus, slot, function, 0);
				device = pci_config_read_word(bus, slot, function, 2);

				if ((vendor != 0xffff) && (vendor != 0x0000))
				{
					status = pci_config_read_word(bus, slot, function, 4);
					subsys_id = pci_config_read_word(bus, slot, function, 0x2c);
					subsys_vendor = pci_config_read_word(bus, slot, function, 0x2e);

					printf("pci: %02d.%02d.%02d = %04x:%04x  (st:%04x)  %04x:%04x\n",
						bus, slot, function, vendor, device, status, subsys_id, subsys_vendor);
				}
			}
		}
	}

	{uint8 attr = con_settextcolor(15, 4);
	printf("FIXME: Implement an ADT to hold the PCI config data.\n");
	con_set_attr(attr); }
}
