/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Accessing args[0] for probes without arguments triggers a fault.
 *
 * SECTION: Variables/Built-in Variables/args
 */

#pragma D option quiet

BEGIN {
	trace(args[0]);
	exit(0);
}

ERROR {
	exit(1);
}
