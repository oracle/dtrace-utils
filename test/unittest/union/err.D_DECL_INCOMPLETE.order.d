/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * When two unions are defined such that one of them contains the other, the
 * inner union has to be defined first.
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
	int position;
	char content;
};

union record rec;
union pirate pir;

BEGIN
{
	rec.content = 'y';
	rec.position = 2;

	pir.content = 'z';
	pir.position = 26;

	printf(
	"rec.content: %c\nrec.position: %d\npir.content: %c\npir.position: %d",
	rec.content, rec.position, pir.content, pir.position);

	exit(0);
}

