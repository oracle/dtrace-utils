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
 * Copyright 2011 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/dtrace_types.h>
#include <time.h>
#include <pthread.h>
#include <mutex.h>

hrtime_t
gethrtime(void)
{
        struct timespec sp;
        int ret;
        long long v;

        ret = clock_gettime(CLOCK_REALTIME, &sp);
        if (ret)
                return 0;

        v = 1000000000LL;
        v *= sp.tv_sec;
        v += sp.tv_nsec;

        return v;
}

int
pthread_cond_reltimedwait_np(pthread_cond_t *cvp, pthread_mutex_t *mp,
        struct timespec *reltime)
{
	struct timespec ts;

	ts = *reltime;
	ts.tv_sec += time(NULL);
	return pthread_cond_timedwait(cvp, mp, &ts);
}

int
mutex_init(mutex_t *m, int flags1, void *ptr)
{
	return pthread_mutex_init(m, NULL);
}

