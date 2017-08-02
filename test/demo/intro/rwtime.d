/*
 * Oracle Linux DTrace.
 * Copyright © 2005, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: readwholedir */

syscall::read:entry,
syscall::write:entry
/pid == $target/
{
	ts[probefunc] = timestamp;
}

syscall::read:return,
syscall::write:return
/pid == $target && ts[probefunc] != 0/
{
	printf("%d nsecs", timestamp - ts[probefunc]);
}

syscall::exit_group:entry
/pid == $target/
{
       exit(0);
}
