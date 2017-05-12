/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma	ident	"%Z%%M%	%I%	%E% SMI"

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
