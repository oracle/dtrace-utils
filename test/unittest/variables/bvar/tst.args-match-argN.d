/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: When no argument mapping is in place, args[N] == argN, modulo
 *	      datatype.
 *
 * SECTION: Variables/Built-in Variables/args
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("echo write something > /dev/null");
}

write:entry {
	printf("%x vs %x\n", args[0], arg0);
	printf("%x vs %x\n", (uint64_t)args[1], arg1);
	printf("%x vs %x\n", args[2], arg2);
	exit(args[0] == arg0 && (uint64_t)args[1] == arg1 && args[2] == arg2 ? 0 : 1);
}

ERROR {
	exit(1);
}
