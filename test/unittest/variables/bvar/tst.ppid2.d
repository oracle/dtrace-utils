/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'ppid' variable matches the scripting variable for BEGIN.
 *
 * SECTION: Variables/Built-in Variables/ppid
 */

#pragma D option quiet

BEGIN {
       trace(ppid);
       trace($ppid);
       exit(ppid == $ppid ? 0 : 1);
}

ERROR {
	exit(1);
}
