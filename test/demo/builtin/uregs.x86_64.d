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
 * Copyright 2011 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

BEGIN
{
	printf("R_GS = 0x%x\n", uregs[R_GS]);
}
BEGIN
{
	printf("R_ES = 0x%x\n", uregs[R_ES]);
}
BEGIN
{
	printf("R_DS = 0x%x\n", uregs[R_DS]);
}
BEGIN
{
	printf("R_RDI = 0x%x\n", uregs[R_RDI]);
}
BEGIN
{
	printf("R_RSI = 0x%x\n", uregs[R_RSI]);
}
BEGIN
{
	printf("R_RBP = 0x%x\n", uregs[R_RBP]);
}
BEGIN
{
	printf("R_RBX = 0x%x\n", uregs[R_RBX]);
}
BEGIN
{
	printf("R_RDX = 0x%x\n", uregs[R_RDX]);
}
BEGIN
{
	printf("R_RCX = 0x%x\n", uregs[R_RCX]);
}
BEGIN
{
	printf("R_RAX = 0x%x\n", uregs[R_RAX]);
/*	printf("R_TRAPNO = 0x%x\n", uregs[R_TRAPNO]); */
/*	printf("R_ERR = 0x%x\n", uregs[R_ERR]); */
}
BEGIN
{
	printf("R_RIP = 0x%x\n", uregs[R_RIP]);
}
BEGIN
{
	printf("R_CS = 0x%x\n", uregs[R_CS]);
}
BEGIN
{
	printf("R_EFL = 0x%x\n", uregs[R_EFL]);
}
BEGIN
{
	printf("R_RSP = 0x%x\n", uregs[R_RSP]);
}
BEGIN
{
	printf("R_SS = 0x%x\n", uregs[R_SS]);
	exit(0);
}
ERROR
{
	printf("uregs access failed.\n");
	exit(1);
}
