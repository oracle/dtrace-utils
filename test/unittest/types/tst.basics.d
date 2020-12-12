/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test declaration processing of all the fundamental kinds of type
 *   declarations.  Check their sizes.
 *
 * SECTION: Types, Operators, and Expressions/Data Types and Sizes
 */


#pragma D option quiet

BEGIN
{
	printf("\nsizeof(char) = %u\n", sizeof(char));
	printf("sizeof(signed char) = %u\n", sizeof(signed char));
	printf("sizeof(unsigned char) = %u\n", sizeof(unsigned char));
	printf("sizeof(short) = %u\n", sizeof(short));
	printf("sizeof(signed short) = %u\n", sizeof(signed short));
	printf("sizeof(unsigned short) = %u\n", sizeof(unsigned short));
	printf("sizeof(int) = %u\n", sizeof(int));
	printf("sizeof(signed int) = %u\n", sizeof(signed int));
	printf("sizeof(unsigned int) = %u\n", sizeof(unsigned int));
	printf("sizeof(long long) = %u\n", sizeof(long long));
	printf("sizeof(signed long long) = %u\n", sizeof(signed long long));
	printf("sizeof(unsigned long long) = %u\n", sizeof(unsigned long long));
	printf("sizeof(float) = %u\n", sizeof(float));
	printf("sizeof(double) = %u\n", sizeof(double));

	exit(0);
}
