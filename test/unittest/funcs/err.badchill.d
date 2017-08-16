/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

BEGIN
{
	chill(200);
	chill(((hrtime_t)1 << 63) - 1);
	exit(0);
}

ERROR
{
	exit(1);
}
