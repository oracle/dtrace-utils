/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@skip: not used directly by the test harness; called by other scripts */

/*
 * For all io::: probes, dump "all" probe arguments (and their interesting members).
 * It would be nice just to say "io:::", but our use of args[] forces us to
 * enumerate the probes.
 */
io:::wait-start,
io:::wait-done,
io:::start,
io:::done
{
	printf("%s: %s: %s: %11s %d %3x  %9d %9d   %p %d %d  %5d %5d    %p   %d %d %d   %d %d %d %s %s %s\n",
	    probeprov, probemod, probefunc, probename,
	    arg1,

	    args[0]->b_flags,

	    args[0]->b_bcount,
	    args[0]->b_bufsize,

	    args[0]->b_addr,
	    args[0]->b_resid,
	    args[0]->b_error,

	    args[0]->b_lblkno,
	    args[0]->b_blkno,

	    args[0]->b_iodone,

	    args[0]->b_edev,
	    getmajor(args[0]->b_edev),
	    getminor(args[0]->b_edev),

	    args[1]->dev_major,
	    args[1]->dev_minor,
	    args[1]->dev_instance,
	    args[1]->dev_name,
	    args[1]->dev_statname,
	    args[1]->dev_pathname);
}
