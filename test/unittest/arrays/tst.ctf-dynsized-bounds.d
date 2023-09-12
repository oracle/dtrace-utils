/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Array accesses work for CTF-declared arrays of dynamic size.
 * 	pr_pgid is one such.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

BEGIN
{
	trace(curpsinfo->pr_pgid);
	exit(0);
}
