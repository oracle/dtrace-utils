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
	printf(" %d", R_X0);
	printf(" %d", R_X1);
	printf(" %d", R_X2);
	printf(" %d", R_X3);
	printf(" %d", R_X4);
	printf(" %d", R_X5);
	printf(" %d", R_X6);
	printf(" %d", R_X7);
	printf(" %d", R_X8);
	printf(" %d", R_X9);
	printf(" %d", R_X10);
	printf(" %d", R_X11);
	printf(" %d", R_X12);
	printf(" %d", R_X13);
	printf(" %d", R_X14);
	printf(" %d", R_X15);
	printf(" %d", R_X16);
	printf(" %d", R_X17);
	printf(" %d", R_X18);
	printf(" %d", R_X19);
	printf(" %d", R_X20);
	printf(" %d", R_X21);
	printf(" %d", R_X22);
	printf(" %d", R_X23);
	printf(" %d", R_X24);
	printf(" %d", R_X25);
	printf(" %d", R_X26);
	printf(" %d", R_X27);
	printf(" %d", R_X28);
	printf(" %d", R_X29);
	printf(" %d", R_X30);
	printf(" %d", R_SP);
	printf(" %d", R_PC);
	printf(" %d", R_PSTATE);
	printf(" %d", R_FP);
	printf("\n");

        exit(0);
}
