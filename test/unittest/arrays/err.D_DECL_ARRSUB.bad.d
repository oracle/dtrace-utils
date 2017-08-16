/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Arrays declarations must have a positive constant as a
 * 	subscription.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

int a[-7];

BEGIN
{
	exit(0);
}
