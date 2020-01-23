/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet
#pragma D option strsize=1024
#pragma D option aggsize=512

BEGIN
{
	@["Harding"] = count();
	@["Hoover"] = count();
	@["Nixon"] = count();
	@["Bush"] = count();
}

BEGIN
{
	exit(0);
}
