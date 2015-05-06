#       kernel/vmw_gate.S
#
#	Implements 'C' callable function to invoke the vmware hypervisor.

.code32
.text

# 'C' function defined as:
# extern void vmware_invoke(struct vmw_regs *regs);
#
# struct vwm_regs { unsigned long int eax, ebc, ecx, edx, esi, edi, ebp };
#

.global	_vmware_invoke
_vmware_invoke:

# Because we clobber EBP, we can't build a standard stack frame.
# GCC requires that we preserve edi, esi, ebx and ebp.

	pushl	%ebp
	pushl	%ebx
	pushl	%edi
	pushl	%esi

# Normal function prolog code would be "push ebp, move esp -> ebp, sub xxx -> esp"
# We have 7 regs to stuff.  Each reg takes 4 bytes, so 28 bytes.

# ESP + 20 = address on stack of the "regs" pointer.
# It would have been ESP + 4, but we pushed 16 bytes above.
	movl	20(%esp), %ebp	# EBP = "regs"

# Load up our regs for the VMWare call.
	movl	0(%ebp), %eax	# EAX = regs->eax
	movl	4(%ebp), %ebx
	movl	8(%ebp), %ecx
	movl	12(%ebp), %edx
	movl	16(%ebp), %esi
	movl	20(%ebp), %edi
	movl	24(%ebp), %ebp

# Call to VMWare.
	in	%dx, %eax

# Save the regs back to the caller's structure.
# Note that NONE of the vmware functions return anything
# significant in the EBP register (the enhanced RPC functions
# us it pass in an RPC cookie).  So we won't play funky games
# trying to recover EBP (we don't have enough registers without
# hijacking ESP or pushing them onto the stack.

	movl	20(%esp), %ebp	# EBP = "regs"
	movl	%eax, 0(%ebp)
	movl	%ebx, 4(%ebp)
	movl	%ecx, 8(%ebp)
	movl	%edx, 12(%ebp)
	movl	%esi, 16(%ebp)
	movl	%edi, 20(%ebp)

	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp
	ret

.size _vmware_invoke, . - _vmware_invoke
