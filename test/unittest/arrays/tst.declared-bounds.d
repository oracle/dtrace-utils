/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	Within-bounds array dereferences work.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

int a[5];

BEGIN
{
	trace(a[0]);
	exit(0);
}
