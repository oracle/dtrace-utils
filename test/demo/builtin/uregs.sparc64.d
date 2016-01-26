/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2016 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
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
