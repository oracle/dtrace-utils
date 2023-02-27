/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Positive test to make sure that we can invoke x86
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
	printf("R_GS = 0x%x\n", uregs[R_GS]);
	printf("R_ES = 0x%x\n", uregs[R_ES]);
	printf("R_DS = 0x%x\n", uregs[R_DS]);
	printf("R_EDI = 0x%x\n", uregs[R_EDI]);
	printf("R_ESI = 0x%x\n", uregs[R_ESI]);
	printf("R_EBP = 0x%x\n", uregs[R_EBP]);
	printf("R_EBX = 0x%x\n", uregs[R_EBX]);
	printf("R_EDX = 0x%x\n", uregs[R_EDX]);
	printf("R_ECX = 0x%x\n", uregs[R_ECX]);
	printf("R_EAX = 0x%x\n", uregs[R_EAX]);
	printf("R_TRAPNO = 0x%x\n", uregs[R_TRAPNO]);
	printf("R_ERR = 0x%x\n", uregs[R_ERR]);
	printf("R_EIP = 0x%x\n", uregs[R_EIP]);
	printf("R_CS = 0x%x\n", uregs[R_CS]);
	printf("R_EFL = 0x%x\n", uregs[R_EFL]);
	printf("R_SS = 0x%x\n", uregs[R_SS]);
	exit(0);
}

ERROR
{
	printf("uregs access failed.\n");
	exit(1);
}
