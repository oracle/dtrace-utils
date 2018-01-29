/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *	Positive test to make sure that we can invoke common
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
	printf("R_PC = 0x%x\n", uregs[R_PC]);
	printf("R_SP = 0x%x\n", uregs[R_SP]);
	exit(0);
}

ERROR
{
	printf("uregs access failed.\n");
	exit(1);
}
