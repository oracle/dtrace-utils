/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'uid' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/uid
 */

#pragma D option quiet

BEGIN {
	trace(uid);
	exit(uid != -1 ? 0 : 1);
}
