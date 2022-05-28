/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Out-of-bounds array accesses work, as long as you cast.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

BEGIN
{
	trace(((char *)curlwpsinfo->pr_name)[32]);
	exit(0);
}
