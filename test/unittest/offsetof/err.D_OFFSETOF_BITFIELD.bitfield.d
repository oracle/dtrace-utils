/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Test invocation of offsetof() with a member that is a bit-field.
 * This should fail at compile time.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 * NOTES:
 *
 */

struct foo {
	int a:1;
	int b:3;
};

BEGIN
{
	trace(offsetof(struct foo, b));
	exit(0);
}
