/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Create inline names from aliases created using typedef.
 *
 * SECTION: Type and Constant Definitions/Inlines
 */

#pragma D option quiet


struct record {
	char c;
	int i;
};

struct record rec1;
inline struct record rec2 = rec1;

union var {
	char c;
	int i;
};

union var un1;
inline union var un2 = un1;


BEGIN
{
	rec1.c = 'c';
	rec1.i = 10;

	un1.c = 'd';

	printf("rec1.c: %c\nrec1.i:%d\nun1.c: %c\n", rec1.c, rec1.i, un1.c);
	printf("rec2.c: %c\nrec2.i:%d\nun2.c: %c\n", rec2.c, rec2.i, un2.c);
	exit(0);
}
