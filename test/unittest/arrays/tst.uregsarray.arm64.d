/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Positive test to make sure that we can invoke ARM64
 *	ureg[] aliases.
 *
 * SECTION: User Process Tracing/uregs Array
 *
 * NOTES: This test does no verification - the value of the output
 *	is not deterministic.
 */

#pragma D option quiet

BEGIN
{
	printf("R_X0 = 0x%x\n", uregs[R_X0]);
	printf("R_X1 = 0x%x\n", uregs[R_X1]);
	printf("R_X2 = 0x%x\n", uregs[R_X2]);
	printf("R_X3 = 0x%x\n", uregs[R_X3]);
	printf("R_X4 = 0x%x\n", uregs[R_X4]);
	printf("R_X5 = 0x%x\n", uregs[R_X5]);
	printf("R_X6 = 0x%x\n", uregs[R_X6]);
	printf("R_X7 = 0x%x\n", uregs[R_X7]);
	printf("R_X8 = 0x%x\n", uregs[R_X8]);
	printf("R_X9 = 0x%x\n", uregs[R_X9]);
	printf("R_X10 = 0x%x\n", uregs[R_X10]);
	printf("R_X11 = 0x%x\n", uregs[R_X11]);
	printf("R_X12 = 0x%x\n", uregs[R_X12]);
	printf("R_X13 = 0x%x\n", uregs[R_X13]);
	printf("R_X14 = 0x%x\n", uregs[R_X14]);
	printf("R_X15 = 0x%x\n", uregs[R_X15]);
	printf("R_X16 = 0x%x\n", uregs[R_X16]);
	printf("R_X17 = 0x%x\n", uregs[R_X17]);
	printf("R_X18 = 0x%x\n", uregs[R_X18]);
	printf("R_X19 = 0x%x\n", uregs[R_X19]);
	printf("R_X20 = 0x%x\n", uregs[R_X20]);
	printf("R_X21 = 0x%x\n", uregs[R_X21]);
	printf("R_X22 = 0x%x\n", uregs[R_X22]);
	printf("R_X23 = 0x%x\n", uregs[R_X23]);
	printf("R_X24 = 0x%x\n", uregs[R_X24]);
	printf("R_X25 = 0x%x\n", uregs[R_X25]);
	printf("R_X26 = 0x%x\n", uregs[R_X26]);
	printf("R_X27 = 0x%x\n", uregs[R_X27]);
	printf("R_X28 = 0x%x\n", uregs[R_X28]);
	printf("R_X29 = 0x%x\n", uregs[R_X29]);
	printf("R_X30 = 0x%x\n", uregs[R_X30]);

	printf("R_FP = 0x%x\n", uregs[R_FP]);
	printf("R_SP = 0x%x\n", uregs[R_SP]);
	printf("R_PC = 0x%x\n", uregs[R_PC]);
	printf("R_PSTATE = 0x%x\n", uregs[R_PSTATE]);
	exit(0);
}

ERROR
{
	printf("uregs access failed.\n");
	exit(1);
}
