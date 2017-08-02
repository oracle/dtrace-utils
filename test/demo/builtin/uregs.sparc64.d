/*
 * Oracle Linux DTrace.
 * Copyright © 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

BEGIN
{
	printf("R_ASI = 0x%x\n", uregs[R_ASI]);
}

BEGIN
{
	printf("R_CCR = 0x%x\n", uregs[R_CCR]);
}

BEGIN
{
	printf("R_FP = 0x%x\n", uregs[R_FP]);
}

BEGIN
{
	printf("R_FPRS = 0x%x\n", uregs[R_FPRS]);
}

BEGIN
{
	printf("R_G0 = 0x%x\n", uregs[R_G0]);
}

BEGIN
{
	printf("R_G1 = 0x%x\n", uregs[R_G1]);
}

BEGIN
{
	printf("R_G2 = 0x%x\n", uregs[R_G2]);
}

BEGIN
{
	printf("R_G3 = 0x%x\n", uregs[R_G3]);
}

BEGIN
{
	printf("R_G4 = 0x%x\n", uregs[R_G4]);
}

BEGIN
{
	printf("R_G5 = 0x%x\n", uregs[R_G5]);
}

BEGIN
{
	printf("R_G6 = 0x%x\n", uregs[R_G6]);
}

BEGIN
{
	printf("R_G7 = 0x%x\n", uregs[R_G7]);
}

BEGIN
{
	printf("R_I0 = 0x%x\n", uregs[R_I0]);
}

BEGIN
{
	printf("R_I1 = 0x%x\n", uregs[R_I1]);
}

BEGIN
{
	printf("R_I2 = 0x%x\n", uregs[R_I2]);
}

BEGIN
{
	printf("R_I3 = 0x%x\n", uregs[R_I3]);
}

BEGIN
{
	printf("R_I4 = 0x%x\n", uregs[R_I4]);
}

BEGIN
{
	printf("R_I5 = 0x%x\n", uregs[R_I5]);
}

BEGIN
{
	printf("R_I6 = 0x%x\n", uregs[R_I6]);
}

BEGIN
{
	printf("R_I7 = 0x%x\n", uregs[R_I7]);
}

BEGIN
{
	printf("R_L0 = 0x%x\n", uregs[R_L0]);
}

BEGIN
{
	printf("R_L1 = 0x%x\n", uregs[R_L1]);
}

BEGIN
{
	printf("R_L2 = 0x%x\n", uregs[R_L2]);
}

BEGIN
{
	printf("R_L3 = 0x%x\n", uregs[R_L3]);
}

BEGIN
{
	printf("R_L4 = 0x%x\n", uregs[R_L4]);
}

BEGIN
{
	printf("R_L5 = 0x%x\n", uregs[R_L5]);
}

BEGIN
{
	printf("R_L6 = 0x%x\n", uregs[R_L6]);
}

BEGIN
{
	printf("R_L7 = 0x%x\n", uregs[R_L7]);
}

BEGIN
{
	printf("R_NPC = 0x%x\n", uregs[R_NPC]);
}

BEGIN
{
	printf("R_O0 = 0x%x\n", uregs[R_O0]);
}

BEGIN
{
	printf("R_O1 = 0x%x\n", uregs[R_O1]);
}

BEGIN
{
	printf("R_O2 = 0x%x\n", uregs[R_O2]);
}

BEGIN
{
	printf("R_O3 = 0x%x\n", uregs[R_O3]);
}

BEGIN
{
	printf("R_O4 = 0x%x\n", uregs[R_O4]);
}

BEGIN
{
	printf("R_O5 = 0x%x\n", uregs[R_O5]);
}

BEGIN
{
	printf("R_O6 = 0x%x\n", uregs[R_O6]);
}

BEGIN
{
	printf("R_O7 = 0x%x\n", uregs[R_O7]);
}

BEGIN
{
	printf("R_PC = 0x%x\n", uregs[R_PC]);
}

BEGIN
{
	printf("R_PS = 0x%x\n", uregs[R_PS]);
}

BEGIN
{
	printf("R_R0 = 0x%x\n", uregs[R_R0]);
}

BEGIN
{
	printf("R_R1 = 0x%x\n", uregs[R_R1]);
}

BEGIN
{
	printf("R_SP = 0x%x\n", uregs[R_SP]);
}

BEGIN
{
	printf("R_Y = 0x%x\n", uregs[R_Y]);
}

BEGIN
{
	printf("R_nPC = 0x%x\n", uregs[R_nPC]);
	exit(0);
}

ERROR
{
	printf("uregs access failed.\n");
	exit(1);
}
