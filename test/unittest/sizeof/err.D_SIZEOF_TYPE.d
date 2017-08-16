/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	sizeof() should handle invalid types
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */

BEGIN
{
	trace(sizeof (void));
	exit(0);
}
