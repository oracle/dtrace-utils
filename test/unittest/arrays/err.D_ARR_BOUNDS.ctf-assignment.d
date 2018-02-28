/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	CTF array dereferences cannot be out of bounds.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

BEGIN
{
	/* This must be higher than TASK_COMM_LEN. */
	curlwpsinfo->pr_name[256] = '0';
	exit(0);
}
