http://www.faqs.org/faqs/assembly-language/x86/general/part1/
http://www.nondot.org/sabre/os/articles
http://www.osdev.org
http://www.osdever.net


// Get a list of all GCC built-in functions.
strings `gcc -print-prog-name=cc1` | grep "^__builtin_" 

============

How to mark a supervisor page read-only so that kernel writes to read-only pages will fault.
<eieio> djenkins: look at CR0.WP

============================================================

filesystem / namespace design:

Actual filesystems are not mounted at "/", but under "/fs".
"/" is pre-mounted to a "/proc" like namespace prior to
the kernel starting user-space programs.

/fs	- where disk and network filesystems are mounted.
/proc	- contains one file for each process, like in BSD, Linux
/dev	- like udev, but kernel manages all listed devices.
	actual device drivers register objects here.
/sys	- read / write objects.


Example filesystem:

/
/dev/ide/0
/dev/ide/0.0	- first partition on first IDE device.
/dev/fdc/0	- first floppy disk.
/dev/scsi/0	- first scsi device.
/dev/eth/0	- first ethernet device.
/dev/con	- console
/dev/null
/dev/zero
/dev/pmem	- raw physical memory


============================================================

All file operations will use 64 bit file pointers.
(attempt) all file operations should be asynchronous.

============================================================

VMWare emulates an "AMD 79c970", "PCnet32 LANCE" ethernet device.
PCI id 1022:2000, rev 10

============================================================

The ".setup" routines do NOT have any fault handlers.  Bad
code will result in lock ups or triple-faults.

If setup locks up PRIOR to enabling paging, then carefully 
check that every function beging called is actually in
".setup.text" and not ".text" (configured for different
virtual addresses).  Same for accessing ".data" and ".bss".

