/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Accessing the members of a struct using the pointer notation using a
 * nonpointer variable throws a D_OP_PTR error.
 *
 * SECTION: Structs and Unions/Pointers to Structs
 *
 */

#pragma D option quiet

struct myinput_struct {
	int i;
	char c;
};

struct myinput_struct f;

BEGIN
{
	f.i = 10;
	f.c = 'c';

	printf("f->i: %d\tf->c: %c\n", f->i, f->c);
	exit(0);
}

ERROR
{
	exit(1);
}
