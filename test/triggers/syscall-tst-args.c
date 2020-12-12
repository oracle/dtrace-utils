/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>
#include <sys/syscall.h>

/*ARGSUSED*/
int
main(int argc, char **argv)
{
	for (;;)
		syscall(SYS_mmap, NULL, 1, 2, 3, -1, 0x12345678UL);

	return 0;
}
