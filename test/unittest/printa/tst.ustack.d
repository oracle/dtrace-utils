/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option maxframes=10

BEGIN
{
	@[ustack()] = count();

	printa("%k\n", @);
	printa("%-20k\n", @);
	printa("%60k\n", @);

	exit(0);
}
