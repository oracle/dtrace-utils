/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: The 'arg2' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/arg2
 */

#pragma D option quiet

BEGIN {
	trace(arg2);
	exit(arg2 != -1 ? 0 : 1);
}
