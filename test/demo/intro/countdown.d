/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

dtrace:::BEGIN
{
	i = 10;
}

profile:::tick-1sec
/i > 0/
{
	trace(i--);
}

profile:::tick-1sec
/i == 0/
{
	trace("blastoff!");
	exit(0);
}
