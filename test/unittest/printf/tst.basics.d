/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the basics of all the format conversions in the printf dictionary.
 *
 * SECTION: Output Formatting/printf()
 *
 * NOTES:
 *  floats and wchar_t strings missing
 */

#pragma D option quiet

BEGIN
{
	i = (int)'a';

	printf("\n");

	printf("%%a = %a\n", &`max_pfn);
	printf("%%c = %c\n", i);
	printf("%%d = %d\n", i);
	printf("%%hd = %hd\n", (short)i);
	printf("%%hi = %hi\n", (short)i);
	printf("%%ho = %ho\n", (ushort_t)i);
	printf("%%hu = %hu\n", (ushort_t)i);
	printf("%%hx = %hx\n", (ushort_t)i);
	printf("%%hX = %hX\n", (ushort_t)i);
	printf("%%i = %i\n", i);
	printf("%%lc = %lc\n", i);
	printf("%%ld = %ld\n", (long)i);
	printf("%%li = %li\n", (long)i);
	printf("%%lo = %lo\n", (ulong_t)i);
	printf("%%lu = %lu\n", (ulong_t)i);
	printf("%%lx = %lx\n", (ulong_t)i);
	printf("%%lX = %lX\n", (ulong_t)i);
	printf("%%o = %o\n", (uint_t)i);
	printf("%%p = %p\n", (void *)i);
	printf("%%s = %s\n", "hello");
	printf("%%u = %u\n", (uint_t)i);
/*	printf("%%wc = %wc\n", i); */
	printf("%%x = %x\n", (uint_t)i);
	printf("%%X = %X\n", (uint_t)i);

	exit(0);
}
