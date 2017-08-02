/*
 * Oracle Linux DTrace.
 * Copyright © 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: needs porting */

sched:::sleep
/curlwpsinfo->pr_stype == SOBJ_SHUTTLE/
{
	bedtime[curlwpsinfo->pr_addr] = timestamp;
}

sched:::wakeup
/execname == "nscd" && bedtime[args[0]->pr_addr]/
{
	@[stringof(curpsinfo->pr_fname), stringof(args[1]->pr_fname)] =
	    quantize(timestamp - bedtime[args[0]->pr_addr]);
	bedtime[args[0]->pr_addr] = 0;
}

sched:::wakeup
/bedtime[args[0]->pr_addr]/
{
	bedtime[args[0]->pr_addr] = 0;
}
