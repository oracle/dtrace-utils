/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'stackdepth' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/stackdepth
 */

#pragma D option quiet

BEGIN {
	trace(stackdepth);
	exit(stackdepth != -1 ? 0 : 1);
}
