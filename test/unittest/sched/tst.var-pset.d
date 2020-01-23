/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: none */

profile-10ms
{
	trace(pset);
	n++;
}

profile-10ms
/n > 2/
{
	exit(0);
}

tick-1s
{
	exit(1);
}
