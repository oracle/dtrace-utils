/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: ustack-tst-basic */

#pragma D option quiet

/*
 * ASSERTION: Verify ustackdepth against the ustack.
 *
 * SECTION: Variables/Built-in Variables
 */

profile-1
/pid == $target/
{
    printf("DEPTH %d\n", ustackdepth);
    printf("TRACE BEGIN\n");
    ustack();
    printf("TRACE END\n");
    exit(0);
}

ERROR
{
	exit(1);
}
