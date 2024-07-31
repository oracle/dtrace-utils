/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	/*
	 * Unless we are actually crashing, the `crashing_cpu int will be -1.
	 * We cast it to int to ensure that the value read from the kernel
	 * is sign-extended.
	 */
	trace((int)`crashing_cpu);
	exit(0);
}

ERROR
{
	exit(1);
}
