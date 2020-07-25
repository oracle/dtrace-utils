/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'arg8' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/arg8
 */

#pragma D option quiet

BEGIN {
	trace(arg8);
	exit(arg8 != -1 ? 0 : 1);
}
