/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Attributes may not be used with void type.
 *
 * SECTION: Errtags/D_DECL_VOIDATTR
 *
 */

#pragma D option quiet

short void ptr;

BEGIN
{
	exit(0);
}

ERROR
{
	exit(0);
}
