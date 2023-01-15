/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

#pragma D option destructive

BEGIN
{
        /* Force some process to exit sharpish. */
	system("true");
}

proc:::exit
{
	exit(0);
}
