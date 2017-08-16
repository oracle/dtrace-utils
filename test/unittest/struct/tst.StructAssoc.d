/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * When a struct is used as a key for an associative array, the key is formed
 * by using the values of the members of the struct variable and not the
 * address of the struct variable.
 *
 * SECTION: Structs and Unions/Structs
 *
 */

#pragma D option quiet

struct record {
	int position;
	char content;
};

struct record r1;
struct record r2;

BEGIN
{
	r1.position = 1;
	r1.content = 'a';

	r2.position = 1;
	r2.content = 'a';

	assoc_array[r1] = 1000;
	assoc_array[r2] = 2000;

	printf("assoc_array[r1]: %d\n", assoc_array[r1]);
	printf("assoc_array[r2]: %d\n", assoc_array[r2]);

	exit(0);
}

END
/assoc_array[r1] != assoc_array[r2]/
{
	printf("Error");
	exit(1);
}
