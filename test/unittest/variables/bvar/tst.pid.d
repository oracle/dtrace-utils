/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'pid' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/pid
 */

#pragma D option quiet

BEGIN {
	trace(pid);
	exit(pid != -1 ? 0 : 1);
}
