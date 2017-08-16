/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Declaring an inner struct within another struct and not defining it should
 * throw a compiler error.
 *
 * SECTION: Structs and Unions/Structs
 *
 */


#pragma D option quiet


struct record {
	struct pirate p;
	int position;
	char content;
};


struct record rec;

BEGIN
{
	rec.position = 0;
	rec.content = 'a';
	printf("rec.position: %d\nrec.content: %c\n",
	 rec.position, rec.content);

	exit(0);
}

