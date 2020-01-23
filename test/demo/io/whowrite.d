/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

io:::start
/args[0]->b_flags & B_WRITE/
{
	@[execname, args[2]->fi_dirname] = count();
}

END
{
	printf("%20s %51s %5s\n", "WHO", "WHERE", "COUNT");
	printa("%20s %51s %5@d\n", @);
}
