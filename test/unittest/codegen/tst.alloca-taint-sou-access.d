/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Check that ALLOCA annotations are propagated correctly across structure
 * member dereferences.
 */

BEGIN
{
	ptr = (struct in6_addr *)alloca(sizeof(struct in6_addr));
	ptr->in6_u.u6_addr8[0] = 0x42;

	exit(0);
}

ERROR
{
	exit(1);
}
