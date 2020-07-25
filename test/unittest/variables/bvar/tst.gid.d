/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'gid' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/gid
 */

#pragma D option quiet

BEGIN {
	trace(gid);
	exit(gid != -1 ? 0 : 1);
}
