/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * To print execname and make sure it succeeds.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

BEGIN
/execname == "dtrace" || execname == "java"/
{
	trace("execname matched");
	exit(0);
}

BEGIN
{
	trace("execname didn't match");
	exit(1);
}

ERROR
{
	exit(1);
}
