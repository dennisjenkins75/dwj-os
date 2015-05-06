/*	kernel/vmware.h */

#ifndef	__VMWARE_H__
#define	__VMWARE_H__

#define VMWARE_MAGIC    0x564d5868
#define VMWARE_RPCI	0x49435052
#define VMWARE_PORT     0x5658

// Set to '1' if we are running inside vmware, 0 if not.
extern int g_vmware_detected;

struct  vmw_regs
{
	unsigned long int eax, ebx, ecx, edx, esi, edi, ebp;
} __attribute__((packed));


/* vmw_gate.S */
extern void vmware_invoke(struct vmw_regs *regs);

/* vmwguest.c */
extern int detect_vmware(void);

extern void vmware_shutdown(void);

extern int vmware_rpc(const char *rpc_name, int rpc_len, char *dest, int dest_len);

#endif	// __VMWARE_H__
