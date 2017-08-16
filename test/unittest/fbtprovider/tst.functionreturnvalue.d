/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Fbt provider return value verify test.
 *
 * SECTION: FBT Provider/Probe arguments
 */

#pragma D option quiet
#pragma D option statusrate=10ms

fbt::SyS_ioctl:return
{
	printf("The function return value is stored in %u\n", arg1);
	exit(0);
}
