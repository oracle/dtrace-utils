/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  	mutex_owned() should return a non-zero value if the calling
 *	thread currently holds the mutex.
 *
 * SECTION: Actions and Subroutines/mutex_owned()
 */

#pragma D option quiet

fbt::mutex_lock:entry
{
	this->mutex = arg0;
}

fbt::mutex_lock:return
{
	this->owned = mutex_owned((struct mutex *)this->mutex);
	this->owner = mutex_owner((struct mutex *)this->mutex);
}

fbt::mutex_lock:return
/!this->owned/
{
	printf("mutex_owned() returned 0, expected non-zero\n");
	exit(1);
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
