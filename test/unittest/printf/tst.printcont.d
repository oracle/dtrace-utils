/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test 'uint' continously; like kinda stress.
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

uint64_t ts;

BEGIN
{
	ts = 53114233149441;
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	exit(0);
}
