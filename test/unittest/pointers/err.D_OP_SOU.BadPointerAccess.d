/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Accessing a pointer to a struct like a non pointer throws a D_OP_SOU.
 *
 * SECTION: Structs and Unions/Pointers to Structs
 *
 */

#pragma D option quiet

struct myinput_struct {
	int i;
	char c;
};

struct myinput_struct *f;
BEGIN
{
	f.i = 10;
	f.c = 'c';

	exit(0);
}
