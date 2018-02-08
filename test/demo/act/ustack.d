/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: readwholedir */
/* @@runtest-opts: -C */

#pragma D option quiet

#if defined(__aarch64__)
syscall::newfstatat:entry
#else
syscall::*lstat:entry
#endif
/pid == $target && ustacked < 3/
{
	ustack();
        ustacked++;
}

syscall::exit_group:entry
/pid == $target/
{
	exit(0);
}
