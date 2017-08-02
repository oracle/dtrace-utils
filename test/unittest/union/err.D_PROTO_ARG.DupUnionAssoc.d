/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Declaring an associative array with a union to be its key type and trying to
 * index with another union having the same composition throws an error.
 *
 * SECTION: Structs and Unions/Unions
 *
 */

#pragma D option quiet

union record {
	int position;
	char content;
};

union pirate {
	int position;
	char content;
};

union record r1;
union record r2;
union pirate p1;
union pirate p2;

BEGIN
{
	r1.position = 1;
	r1.content = 'a';

	r2.position = 2;
	r2.content = 'b';

	p1.position = 1;
	p1.content = 'a';

	p2.position = 2;
	p2.content = 'b';

	assoc_array[r1] = 1000;
	assoc_array[r2] = 2000;
	assoc_array[p1] = 3333;
	assoc_array[p2] = 4444;

	printf("assoc_array[r1]: %d\n",  assoc_array[r1]);
	printf("assoc_array[r2]: %d\n",  assoc_array[r2]);
	printf("assoc_array[p1]: %d\n",  assoc_array[p1]);
	printf("assoc_array[p2]: %d\n",  assoc_array[p2]);

	exit(0);
}
