/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Recursive naming of unions should produce compiler error.
 *
 * SECTION: Structs and Unions/Unions
 *
 */

#pragma D option quiet

union record {
	union record rec;
	int position;
	char content;
};

union record r1;
union record r2;

BEGIN
{
	r1.position = 1;
	r1.content = 'a';

	r2.position = 2;
	r2.content = 'b';

	printf("r1.position: %d\nr1.content: %c\n", r1.position, r1.content);
	printf("r2.position: %d\nr2.content: %c\n", r2.position, r2.content);

	exit(0);
}
