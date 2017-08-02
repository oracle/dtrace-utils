/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: $_pid */
/* @@xfail: needs trigger */

syscall::open:entry
/pid==$1/
{
	self->spec = speculation();
}

syscall:::
/self->spec && pid==$1/
{
	speculate(self->spec);
	printf("this is speculative");
}
