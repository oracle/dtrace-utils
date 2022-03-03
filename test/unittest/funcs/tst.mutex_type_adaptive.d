/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	mutex_type_adaptive() should return a non-zero value if the
 *	mutex is an adaptive one.
 *
 * SECTION: Actions and Subroutines/mutex_type_adaptive()
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
	this->adaptive = mutex_type_adaptive((struct mutex *)this->mutex);
}

fbt::mutex_lock:return
/!this->adaptive/
{
	printf("mutex_type_adaptive returned 0, expected non-zero\n");
	exit(1);
}

fbt::mutex_lock:return
{
	exit(0);
}
