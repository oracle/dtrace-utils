/*
 * Oracle Linux DTrace.
 * Copyright © 2011, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: testprobe */

#pragma D option quiet

dt_test:::test
{
        @counts["number of times fired"] = count();
}

syscall::exit_group:entry
/ pid == $target /
{
	exit(0);
}
