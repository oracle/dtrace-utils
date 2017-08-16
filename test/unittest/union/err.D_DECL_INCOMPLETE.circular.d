/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * A circular definition of unions (two unions defining each other as
 * members) should throw an error at compile time.
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


union pirate {
	union record r;
	int position;
	char content;
};

union record rec;
union pirate pir;

BEGIN
{
	rec.position = 0;
	rec.content = 'a';
	printf(
	"rec.position: %d\nrec.content: %c\n", rec.position, rec.content);

	exit(0);
}

