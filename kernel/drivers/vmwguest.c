/*	kernel/vmwguest.c

	Implements code to communicate with the VMWare hypervisor if
	this kernel is running as a guest OS inside VMWare.

	http://www.codeproject.com/system/VmDetect.asp
	http://chitchat.at.infoseek.co.jp/vmware/backdoor.html
*/

#include "kernel/kernel.h"

// A global!
int	g_vmware_detected = 0;

/* Returns '1' if running in VMWare, '0' if not. */

int	detect_vmware(void)
{
	struct vmw_regs regs;
	int 	in_vmware = 0;
	int	vmware_version = 0;
	int	product_type = 0;
	uint8	attr = 0;

// http://chitchat.at.infoseek.co.jp/vmware/backdoor.html#cmd0ah

	regs.eax = VMWARE_MAGIC;	// Magic number.
	regs.ebx = 0;			// Anythung BUT the magic number.
	regs.ecx = 10;			// VMWare "get version" command.
	regs.edx = VMWARE_PORT;		// Which IO port triggers the hypervisor.
	regs.esi = 0;			// Not used.
	regs.edi = 0;			// Not used.
	regs.ebp = 0;			// Not used.

	vmware_invoke(&regs);

	in_vmware = regs.ebx == VMWARE_MAGIC;
	vmware_version = in_vmware ? regs.eax : 0;
	product_type = in_vmware ? regs.ecx : 0;

	attr = con_set_attr(0x1e);	// yellow on blue
	printf("VMWare: detected:%p, version:%p, product:%p\n", in_vmware, vmware_version, product_type);
	con_set_attr(attr);

	g_vmware_detected = in_vmware;

	return 0 != in_vmware;
}

void	vmware_shutdown(void)
{
	struct vmw_regs regs;

	regs.eax = VMWARE_MAGIC;
	regs.ebx = 7;			// APM power off.
	regs.ecx = 0x00070002;
	regs.edx = VMWARE_PORT;

	vmware_invoke(&regs);
}

/* Opens a VMWare RPC channel (legacy mode).
   Returns -1 on error or channel # (0-7) on success.
*/
static int vmware_rpc_open(void)
{
	struct vmw_regs regs;

	regs.eax = VMWARE_MAGIC;
	regs.ebx = VMWARE_RPCI;
	regs.ecx = 0x0000001e;		// '0000' = open channel, '001e' = RPC
	regs.edx = VMWARE_PORT;

	vmware_invoke(&regs);

	if (regs.ecx != 0x00010000)
	{
		return -1;
	}

	return regs.edx >> 16;	// channel in high-word of EDX.
}

/* Closes a VMWare RPC channel.  Returns -1 on error and 0 on success. */
static int vmware_rpc_close(int channel)
{
	struct vmw_regs regs;

	regs.eax = VMWARE_MAGIC;
	regs.ebx = VMWARE_RPCI;		// Docs say "don't care"...
	regs.ecx = 0x0006001e;		// '0006' = close channel, '001e' = RPC
	regs.edx = VMWARE_PORT | (channel << 16);

	vmware_invoke(&regs);

	return regs.edx == 0x00010000;	// success code.
}

/* Sends RPC command len, followed by command.
   WARNING: Will possibly read beyond the end of cmd by 3 bytes.
   If "cmd" is dword aligned and allocation granularity is dword,
   then this should not matter. */
static int vmware_rpc_send(int channel, const char *cmd, int cmd_len)
{
	struct vmw_regs regs;
	int index = 0;

// Step #1, sanity check and set string len if caller did not.
	if (!cmd) return -1;
	if (!cmd_len) cmd_len = strlen(cmd);

// Step #2, send command len to vmware.
	regs.eax = VMWARE_MAGIC;
	regs.ebx = cmd_len;
	regs.ecx = 0x0001001e;		// '0001' = send cmd len.
	regs.edx = VMWARE_PORT | (channel << 16);

	vmware_invoke(&regs);

	if (regs.ecx != 0x00810000)
	{
		return -1;
	}

// Step #3, send command string.

	for (index = 0; index < cmd_len; index += 4)
	{
		regs.eax = VMWARE_MAGIC;
		regs.ebx = *(int*)(cmd + index);
		regs.ecx = 0x0002001e;		// '0002' = send cmd bytes.
		regs.edx = VMWARE_PORT | (channel << 16);

		vmware_invoke(&regs);

		if (regs.ecx != 0x00010000)
		{
			return -1;
		}
	}

	return 0;
}

/* Gets the RPC result.  Saves into caller supplied buffer, up to 'dest_len' bytes.
   Returns # of bytes saved, not counting the trailing NULL.  Or returns -1 on error.
   If result buffer is too small, result will be truncated.
*/

static int vmware_rpc_recv(int channel, char *dest, int dest_len)
{
	int index = 0;
	int recv_len = 0;
	int reply_id = 0;
	struct vmw_regs regs;

// Step #1, get the response length from vmware.
	regs.eax = VMWARE_MAGIC;
	regs.ebx = 0;			// Don't care.
	regs.ecx = 0x0003001e;		// '0003' = recv len.
	regs.edx = VMWARE_PORT | (channel << 16);

	vmware_invoke(&regs);

	if (regs.ecx != 0x00830000)
	{
		return -1;
	}

	recv_len = regs.ebx;
	reply_id = regs.edx >> 16;

// Step #2, get the reply, 4 bytes at a time.
	while ((index < recv_len) && (index < dest_len))
	{
		regs.eax = VMWARE_MAGIC;
		regs.ebx = reply_id;
		regs.ecx = 0x0004001e;		// '0004' = recv data
		regs.edx = VMWARE_PORT | (channel << 16);

		vmware_invoke(&regs);

		if (regs.ecx != 0x00010000)
		{
			return -1;
		}

		*(int*)(dest + index) = regs.ebx;

		index += 4;
	}

// If we can, null terminate the string.
	if (recv_len < dest_len)
	{
		dest[recv_len] = 0;
	}

// Close the reply id.
	regs.eax = VMWARE_MAGIC;
	regs.ebx = reply_id;
	regs.ecx = 0x0005001e;
	regs.edx = VMWARE_PORT | (channel << 16);

	vmware_invoke(&regs);

	if (regs.ecx != 0x00010000)
	{
		return -1;
	}

	return index;
}

/*	Uses the vmware backdoor to perform legacy VMWare RPC.  Can be used to log stuff
	to VMWare's log file, set and get guestinfo parameters, set the tools version, etc...

	On input:
		'rpc_name' should point to the RPC command name string, like "tools.set.version a.b.c",
		'rpc_len' should be the number of bytes to in the RPC command name string, or '0' if
			we should just use "strlen" to figure it out (some RPC commands are binary).
		'dest' should point to a user allocated buffer to hold the RPC response string.
		'dest_len' is the max # of bytes in the buffer that we can write to.

	Return value: -1 on error (if we can even catch the error), or the # of bytes actually copied
		into 'dest'.
*/


int	vmware_rpc(const char *rpc_name, int rpc_len, char *dest, int dest_len)
{
	int channel = 0;	// assigned by vmware.
	int recv_len = 0;

	if (!rpc_name || !dest) return -1;
	if (!rpc_len) rpc_len = strlen(rpc_name);

	if (-1 == (channel == vmware_rpc_open()))
	{
		return -1;
	}

	if (-1 == vmware_rpc_send(channel, rpc_name, rpc_len))
	{
		vmware_rpc_close(channel);
		return -1;
	}

	if (-1 == (recv_len = vmware_rpc_recv(channel, dest, dest_len)))
	{
		vmware_rpc_close(channel);
		return -1;
	}

	if (-1 == vmware_rpc_close(channel))
	{
		return -1;
	}

	return recv_len;
}


/*
	if (detect_vmware())
	{
		char dest[100];
		int r = vmware_rpc("log <abc123>", 0, dest, sizeof(dest));
//		printf("response = (%d), '%s'.\n", r, dest);

		r = vmware_rpc("tools.set.version 99999", 0, dest, sizeof(dest));
//		printf("response = (%d), '%s'.\n", r, dest);

		r = vmware_rpc("info-set guestinfo.ip 192.168.0.50", 0, dest, sizeof(dest));
//		printf("response = (%d), '%s'.\n", r, dest);

		r = vmware_rpc("machine.id.get", 0, dest, sizeof(dest));
//		printf("machine.id.get = (%d), '%s'.\n", r, dest);
	}
*/
