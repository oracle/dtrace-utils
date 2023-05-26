/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: requires libctf (binutils) patch */

/*
 * ASSERTION:
 *	Check for the absence of a libctf bug breaking offsets of
 *	members of nested unnamed structs in the kernel.
 *	See https://sourceware.org/bugzilla/show_bug.cgi?id=30264
 *
 */

#pragma D option quiet

BEGIN {
	printf("offset is %i\n", offsetof(struct rds`rds_message, atomic));
}

BEGIN
/ offsetof(struct rds`rds_message, atomic) == 0 /
{
	exit(1);
}

BEGIN
/ offsetof(struct rds`rds_message, atomic) != 0 /
{
	exit(0);
}
