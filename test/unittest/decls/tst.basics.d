/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test a variety of extern declarations that exercise the different
 *  kinds of declarations that we can process.
 *
 * SECTION:  Program Structure/Probe Clauses and Declarations
 *
 */

extern void *e1;
extern char e2;
extern signed char e3;
extern unsigned char e4;
extern short e5;
extern signed short e6;
extern unsigned short e7;
extern int e8;
extern e9;
extern signed int e10;
extern unsigned int e11;
extern long e12;
extern signed long e13;
extern unsigned long e14;
extern long long e15;
extern signed long long e16;
extern unsigned long long e17;
extern float e18;
extern double e19;
extern long double e20;
extern cpuinfo_t e21;
extern struct inode e22;
extern union sigval e23;
extern enum uio_rw e24;

BEGIN
{
	exit(0);
}
