/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: mmap */

syscall::read:entry,
syscall::write:entry
/ pid == $target /
{
	printf("%s(%d, 0x%x, %4d)", probefunc, arg0, arg1, arg2);
}

syscall::read:return, syscall::write:return
/ pid == $target /
{
	printf("\t\t = %d\n", arg1);
}

syscall::exit_group:entry
/ pid == $target /
{
	exit(0);
}
