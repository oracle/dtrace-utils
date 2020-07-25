/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'arg9' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/arg9
 */

#pragma D option quiet

BEGIN {
	trace(arg9);
	exit(arg9 != -1 ? 0 : 1);
}
