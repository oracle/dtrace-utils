/*
 * Oracle Linux DTrace.
 * Copyright © 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

/*
 * ASSERTION:
 *	Verify that array types are really emitted as array types, and
 *	not as their base type.
 */

#pragma D option quiet

BEGIN
{
	printf("%s\n", curthread->comm);
	exit(0);
}
