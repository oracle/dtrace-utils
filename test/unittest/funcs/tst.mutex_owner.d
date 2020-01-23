/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *	mutex_owner() should return a pointer to the kernel thread holding
 *	the mutex.
 *
 * SECTION: Actions and Subroutines/mutex_owner()
 */

#pragma D option quiet

fbt::mutex_lock:entry
{
	this->mutex = arg0;
}

fbt::mutex_lock:return
{
	this->owner = mutex_owner((struct mutex *)this->mutex);
}

fbt::mutex_lock:return
/this->owner != curthread/
{
	printf("current thread is not current owner of owned lock\n");
	exit(1);
}

fbt::mutex_lock:return
{
	exit(0);
}
