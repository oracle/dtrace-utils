/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./204built-in-variables-uregs.d
 *
 *  DESCRIPTION
 *    The uregs[] array can be used to access the current thread's
 *    saved user-mode register values at probe firing time.  The
 *    available registers depend on the chip architecture:
 *
 *    - x86_64
 *        R_SP R_PC R_FP R_TRAPNO R_ERR
 *        R_CS R_DS R_ES R_FS R_GS R_PS R_SS
 *        R_RSP R_EBP R_RFL R_RIP R_RDI R_RSI R_RAX R_RBX R_RCX R_RDX
 *        R_ESP R_RBP R_EFL R_EIP R_EDI R_ESI R_EAX R_EBX R_ECX R_EDX
 *        R_R0 R_R1 R_R8 R_R9 R_R10 R_R11 R_R12 R_R13 R_R14 R_R15
 *
 *    - aarch64:
 *        R_SP R_PC R_PSTATE R_FP
 *        R_X0 R_X1 R_X2 R_X3 R_X4 R_X5 R_X6 R_X7 R_X8 R_X9
 *        R_X10 R_X11 R_X12 R_X13 R_X14 R_X15 R_X16 R_X17 R_X18 R_X19
 *        R_X20 R_X21 R_X22 R_X23 R_X24 R_X25 R_X26 R_X27 R_X28 R_X29
 *        R_X30
 */

BEGIN
{
	printf("0x%x 0x%x\n", uregs[R_PC], uregs[R_SP]);
	exit(0);
}
