/*
 * Oracle Linux DTrace.
 * Copyright © 2005, 2011, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: $_pid */
/* @@xfail: needs trigger */

syscall::open:entry
/pid == $1/
{
	self->path = copyinstr(arg0);
}

syscall::open:return
/self->path != NULL && arg1 == -1/
{
	printf("open for '%s' failed", self->path);
	ustack();
}
