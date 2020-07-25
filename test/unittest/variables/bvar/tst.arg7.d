/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'arg7' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/arg7
 */

#pragma D option quiet

BEGIN {
	trace(arg7);
	exit(arg7 != -1 ? 0 : 1);
}
