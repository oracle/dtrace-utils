/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Thread-local variables can be declared with a struct type and can
 *	      be assigned to.
 *
 * SECTION: Variables/Thread-Local Variables
 */

self struct task_struct tsk;

BEGIN
{
	self->tsk = *curthread;

	exit(self->tsk.pid == pid ? 0 : 1);
}

ERROR
{
	exit(1);
}
