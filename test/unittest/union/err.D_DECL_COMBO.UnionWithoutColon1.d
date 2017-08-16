/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Combining multiple union definitions in a single line should throw a
 * compiler error.
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
}

union record rec;
union pirate pir;

BEGIN
{
	rec.content = 'a';
	rec.position = 1;

	pir.content = 'b';
	pir.position = 2;

	printf(
	"rec.content: %c\nrec.position: %d\npir.content: %c\npir.position: %d",
	rec.content, rec.position, pir.content, pir.position);

	exit(0);
}

