/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the printa() default output format.
 *
 * SECTION: Output Formatting/printa()
 *
 */

#pragma D option quiet

BEGIN
{
	@a = count();
	printa(@a);
	exit(0);
}
