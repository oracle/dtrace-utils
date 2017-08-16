/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 * 	Invalid tuple types result in a DT_DECL_ARRTYPE error.
 *
 * SECTION:
 * 	Pointers and Arrays/Array Declarations and Storage
 */


int x[void, char];

BEGIN
{
	x[trace(), 'a'] = 456;
}
