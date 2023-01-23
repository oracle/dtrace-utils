/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Results for tracemem() are correct.
 *
 * SECTION: Actions and Subroutines/tracemem()
 */

#pragma D option quiet

long long *p;

BEGIN {
	/* pick some pointer to kernel memory */
	p = (long long *) curthread;

	/* dump memory at this pointer */
	printf("%16.16x %16.16x\n", p[ 0], p[ 1]);
	printf("%16.16x %16.16x\n", p[ 2], p[ 3]);
	printf("%16.16x %16.16x\n", p[ 4], p[ 5]);
	printf("%16.16x %16.16x\n", p[ 6], p[ 7]);
	printf("%16.16x %16.16x\n", p[ 8], p[ 9]);
	printf("%16.16x %16.16x\n", p[10], p[11]);
	printf("%16.16x %16.16x\n", p[12], p[13]);
	printf("%16.16x %16.16x\n", p[14], p[15]);
	printf("%16.16x %16.16x\n", p[16], p[17]);
	printf("%16.16x %16.16x\n", p[18], p[19]);
	printf("%16.16x %16.16x\n", p[20], p[21]);
	printf("%16.16x %16.16x\n", p[22], p[23]);
	printf("%16.16x %16.16x\n", p[24], p[25]);
	printf("%16.16x %16.16x\n", p[26], p[27]);
	printf("%16.16x %16.16x\n", p[28], p[29]);
	printf("%16.16x %16.16x\n", p[30], p[31]);

	/* try tracemem() with this same pointer but various sizes */

	tracemem(p, 256);	/* 256 bytes */

	tracemem(p, 256,  -1);	/* 256 bytes (arg2 < 0) */

	tracemem(p, 256,   0);	/*   0 bytes */

	tracemem(p, 256,  32);	/*  32 bytes */

	tracemem(p, 256,  64);	/*  64 bytes */

	tracemem(p, 256, 128);	/* 128 bytes */

	tracemem(p, 256, 256);	/* 256 bytes */

	tracemem(p, 256, 320);	/* 256 bytes (arg1 <= arg2) */

	tracemem(p, 256, (unsigned char) 0x80);
				/* 128 bytes */

	tracemem(p, 256, (signed char) 0x80);
				/* 256 bytes (arg2 < 0) */

	exit(0);
}
