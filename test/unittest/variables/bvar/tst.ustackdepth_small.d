/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: ustack-tst-basic */

#pragma D option quiet
#pragma D option maxframes=5

/*
 * ASSERTION: Verify ustackdepth is limited by option "maxframes".
 *
 * SECTION: Variables/Built-in Variables
 */

profile-1
/pid == $target/
{
    printf("DEPTH %d\n", ustackdepth);
    exit(0);
}

ERROR
{
	exit(1);
}
