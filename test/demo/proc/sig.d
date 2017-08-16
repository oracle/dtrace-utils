/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

proc:::signal-send
{
	@[execname, stringof(args[1]->pr_fname), args[2]] = count();
}

END
{
	printf("%20s %20s %12s %s\n",
	    "SENDER", "RECIPIENT", "SIG", "COUNT");
	printa("%20s %20s %12d %@d\n", @);
}
