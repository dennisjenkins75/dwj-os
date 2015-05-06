/*	kernel/ata.h	*/

// IO Port base address fro default ATA (ide) controllers.
#define	ATA_CTRLR_0_BASE	0x1f0
#define	ATA_CTRLR_1_BASE	0x170

// Relative IO port addresses.
#define ATA_IO_DATA		0
#define ATA_IO_ERROR		1
#define ATA_IO_FEATURES		1
#define ATA_IO_SECTOR_COUNT	2
#define ATA_IO_LBA_0_7		3
#define ATA_IO_LBA_8_15		4
#define ATA_IO_LBA_16_23	5
#define ATA_IO_LBA_24_27	6
#define ATA_IO_STATUS		7
#define ATA_IO_COMMAND		7

// for older CHS methods
#define ATA_IO_DRIVE_HEAD	ATA_IO_LBA_24_27
#define ATA_IO_SECTOR		ATA_IO_LBA_0_7
#define ATA_IO_CYL_LOW		ATA_IO_LBA_8_15
#define ATA_IO_CYL_HIGH		ATA_IO_LBA_16_23

// Status register bitmask
#define ATA_STATUS_ERR		0x01
#define ATA_STATUS_IDX		0x02
#define ATA_STATUS_CORR		0x04
#define ATA_STATUS_DRQ		0x08
#define ATA_STATUS_DSC		0x10
#define ATA_STATUS_DWF		0x20
#define ATA_STATUS_READY	0x40
#define ATA_STATUS_BUSY		0x80


// Mandatory ATA commands
#define ATA_CMD_EXEC_DIAG		0x90
#define ATA_CMD_FORMAT_TRACK		0x50
#define ATA_CMD_INIT_PARAMS		0x91
#define ATA_CMD_READ_LONG		0x22
#define ATA_CMD_READ_LONG_WO		0x23
#define ATA_CMD_READ_SECTORS		0x20
#define ATA_CMD_READ_SECTORS_WO		0x21
#define ATA_CMD_READ_VRFY		0x40
#define ATA_CMD_READ_VRFY_WO		0x41
#define ATA_CMD_RECALIBRATE		0x10
#define ATA_CMD_SEEK			0x70
#define ATA_CMD_WRITE_LONG		0x32
#define ATA_CMD_WRITE_LONG_WO		0x33
#define ATA_CMD_WRITE_SECTORS		0x30
#define ATA_CMD_WRITE_SECTORS_WO	0x31

// Optional ATA commands
/*
  ; ..................................................
  ; OIDE_CMD = Optional IDE Command
  OIDE_CMD_ACKNOWLEDGEMEDIACHANGE     EQU       0x000000DB
  OIDE_CMD_BOOTPOSTBOOT               EQU       0x000000DC
  OIDE_CMD_BOOTPREBOOT                EQU       0x000000DD
  OIDE_CMD_CHECKPOWERMODE             EQU       0x000000E5
  OIDE_CMD_DOORLOCK                   EQU       0x000000DE
  OIDE_CMD_DOORUNLOCK                 EQU       0x000000DF
  OIDE_CMD_IDENTIFYDRIVE              EQU       0x000000EC
  OIDE_CMD_IDENTIFYDEVICE             EQU       0x000000EC
  OIDE_CMD_IDLE                       EQU       0x000000E3
  OIDE_CMD_IDLEIMMEDIATE              EQU       0x000000E1
  OIDE_CMD_NOP                        EQU       0x00000000
  OIDE_CMD_READBUFFER                 EQU       0x000000E4
  OIDE_CMD_READDMAWR                  EQU       0x000000C8
  OIDE_CMD_READDMAWOR                 EQU       0x000000C9
  OIDE_CMD_READMULTIPLE               EQU       0x000000C4
  OIDE_CMD_SETFEATURES                EQU       0x000000EF
  OIDE_CMD_SETMULTIPLEMODE            EQU       0x000000C6
  OIDE_CMD_SLEEP                      EQU       0x000000E6
  OIDE_CMD_STANDBY                    EQU       0x000000E2
  OIDE_CMD_STANDBYIMMEDIATE           EQU       0x000000E0
  OIDE_CMD_WRITEBUFFER                EQU       0x000000C8
  OIDE_CMD_WRITEDMAWR                 EQU       0x000000CA
  OIDE_CMD_WRITEDMAWOR                EQU       0x000000CB
  OIDE_CMD_WRITEMULTIPLE              EQU       0x000000C5
  OIDE_CMD_WRITESAME                  EQU       0x000000E9
  OIDE_CMD_WRITEVERIFY                EQU       0x0000003C
*/

extern void ata_test(void);

