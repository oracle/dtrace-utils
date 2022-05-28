/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Within-bounds array accesses work.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

BEGIN
{
	trace(curlwpsinfo->pr_name[0]);
	exit(0);
}
