/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: mmap */

BEGIN
{
	prot[0] = "---";
	prot[1] = "r--";
	prot[2] = "-w-";
	prot[3] = "rw-";
	prot[4] = "--x";
	prot[5] = "r-x";
	prot[6] = "-wx";
	prot[7] = "rwx";
}

syscall::write:entry
/pid == $target/
{
	fd = arg0;
}

syscall::mmap:entry
/pid == $target && arg4 == fd/
{
	printf("mmap with prot = %s", prot[arg2 & 0x7]);
}

syscall::exit_group:entry
/pid == $target/
{
	exit(0);
}
