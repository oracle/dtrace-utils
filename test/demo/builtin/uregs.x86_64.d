/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
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
