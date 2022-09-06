/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'probeprov' variable returns the correct value.
 *
 * SECTION: Variables/Built-in Variables/probeprov
 */

#pragma D option quiet

BEGIN
{
	trace(probeprov);
}

BEGIN
/probeprov == "dtrace"/
{
	exit(0);
}

BEGIN
{
	exit(1);
}

ERROR {
	exit(1);
}
