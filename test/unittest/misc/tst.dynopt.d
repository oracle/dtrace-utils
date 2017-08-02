/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@skip: known erratic */

#pragma D option quiet

#pragma D option switchrate=1ms
#pragma D option aggrate=1ms

tick-100ms
{
	i++;
}

tick-100ms
/i > 1/
{
	setopt("quiet", "no");
	setopt("quiet");
	setopt("quiet");
	setopt("quiet", "yes");
	@["abc"] = count();
	printa("%@d\n", @);
}

tick-100ms
/i == 5/
{
	setopt("switchrate", "5sec");
	setopt("aggrate", "5sec");
}

tick-100ms
/i == 31/
{
	exit(0);
}
