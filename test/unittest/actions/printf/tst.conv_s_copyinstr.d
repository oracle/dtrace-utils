/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf action supports '%s' for copyinstr()'d values.
 *
 * SECTION: Actions/printf()
 */
/* @@trigger: delaydie */

#pragma D option quiet

syscall::write:entry
/pid == $target/
{
	printf("'%s'", copyinstr(arg1, 32));
	exit(0);
}

ERROR
{
	exit(1);
}
