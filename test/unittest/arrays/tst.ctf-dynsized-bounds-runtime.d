/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Array accesses work for CTF-declared arrays of dynamic size
 *	      (ensuring the bounds checking is also bypassed at runtime).
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

BEGIN
{
	i = pid - pid;			/* Non-constant 0 value. */
	trace(`__start_BTF[i]);
	exit(0);
}

ERROR
{
	exit(1);
}
