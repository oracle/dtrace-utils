/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@skip: Solaris-specific, unconverted */

syscall:::entry
/execname == "find"/
{
	self->syscall = probefunc;
	self->insys = 1;
}

sysinfo:::xcalls
/execname == "find"/
{
	@[self->insys ? self->syscall : "<none>"] = count();
}

syscall:::return
/self->insys/
{
	self->insys = 0;
	self->syscall = NULL;
}
