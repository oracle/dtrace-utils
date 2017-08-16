/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Combining multiple struct definitions in a single line should throw a
 * compiler error.
 *
 * SECTION: Structs and Unions/Structs
 *
 */

#pragma D option quiet

struct record {
	int position;
	char content;
};


struct pirate {
	int position;
	char content;
}

struct record rec;
struct pirate pir;

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

