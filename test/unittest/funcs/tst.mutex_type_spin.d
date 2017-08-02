/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	mutex_type_spin() should return a non-zero value if the
 *	mutex is an spin one.
 *
 * SECTION: Actions and Subroutines/mutex_type_spin()
 *
 * On Linux, all mutexes are adaptive.
 */

#pragma D option quiet

fbt::mutex_lock:entry
{
	this->mutex = arg0;
}

fbt::mutex_lock:return
{
	this->spin = mutex_type_spin((struct mutex *)this->mutex);
}

fbt::mutex_lock:return
/this->spin/
{
	printf("mutex_type_spin returned non-zero, expected 0\n");
	exit(1);
}

fbt::mutex_lock:return
{
	exit(0);
}
