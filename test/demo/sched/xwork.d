/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: needs porting */

self int last;

sched:::wakeup
/self->last && args[0]->pr_stype == SOBJ_CV/
{
	@[stringof(args[1]->pr_fname)] = sum(vtimestamp - self->last);
	self->last = 0;
}

sched:::wakeup
/execname == "Xsun" && self->last == 0/
{
	self->last = vtimestamp;
}
