/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Call fbt provider over non existent function and make
 * sure it results in compilation error.
 *
 * SECTION: FBT Provider/Probes
 */

#pragma D option quiet

BEGIN
{
	self->traceme = 1;
}

void bar();

fbt::bar:entry
{
	printf("Entering the function\n");
}

fbt::bar:return
{
	printf("Returning the function\n");
	exit(0);
}

