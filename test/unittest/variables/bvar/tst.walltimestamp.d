/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'walltimestamp' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/walltimestamp
 */

#pragma D option quiet

BEGIN {
	trace(walltimestamp);
	exit(walltimestamp != -1 ? 0 : 1);
}

ERROR {
	exit(1);
}
