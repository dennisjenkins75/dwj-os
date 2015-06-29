/* kernel/kernel/debug.h */

// Values for "nLevel"
static const int DEBUG_FATAL = 0;
static const int DEBUG_ERROR = 1;
static const int DEBUG_WARN  = 2;
static const int DEBUG_INFO  = 3;
static const int DEBUG_DEBUG = 4;
static const int DEBUG_TRACE = 5;

// Values for "nFacility"
static const int FAC_GENERAL = 0;
static const int FAC_HEAP    = 1;
static const int FAC_OBJECT  = 2;
static const int FAC_KEYBOARD = 3;
static const int FAC_DISK_ATA = 4;
static const int FAC_SPINLOCK = 5;

void	kdebug (int nLevel, int nFacility, const char *fmt, ...);
void	kdebug_mem_dump (int nLevel, int nFacility, const void *addr, uint32 bytes);
