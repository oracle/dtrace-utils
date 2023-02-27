/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  Check the constants used to access uregs[].
 *
 * SECTION: User Process Tracing/uregs Array
 */

#pragma D option quiet

BEGIN
{
	printf(" %d", R_PC);
	printf(" %d", R_SP);
	printf(" %d", R_R0);
	printf(" %d", R_R1);
	printf(" %d", R_CS);
	printf(" %d", R_GS);
	printf(" %d", R_ES);
	printf(" %d", R_DS);
	printf(" %d", R_EDI);
	printf(" %d", R_ESI);
	printf(" %d", R_EBP);
	printf(" %d", R_EAX);
	printf(" %d", R_ESP);
	printf(" %d", R_EAX);
	printf(" %d", R_EBX);
	printf(" %d", R_ECX);
	printf(" %d", R_EDX);
	printf(" %d", R_TRAPNO);
	printf(" %d", R_ERR);
	printf(" %d", R_EIP);
	printf(" %d", R_CS);
	printf(" %d", R_EFL);
	/* printf(" %d", R_UESP); */ /* in the User's Guide, but not defined in regs.d */
	printf(" %d", R_SS);
	printf("\n");
        exit(0);
}
