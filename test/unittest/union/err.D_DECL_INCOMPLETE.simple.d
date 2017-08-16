/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Declaring an inner union without defining it else where should throw
 * a compiler error.
 *
 * SECTION: Structs and Unions/Unions
 *
 */
#pragma D option quiet


union record {
	union pirate p;
	int position;
	char content;
};


union record rec;

BEGIN
{
	rec.position = 0;
	rec.content = 'a';
	printf("rec.position: %d\nrec.content: %c\n",
	rec.position, rec.content);

	exit(0);
}

