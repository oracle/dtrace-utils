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

#ifndef MUTEX_H
#define MUTEX_H
#include <pthread.h>

#define mutex_t pthread_mutex_t

#define DEFAULTMUTEX PTHREAD_MUTEX_INITIALIZER

#define mutex_lock(mp)	 	pthread_mutex_lock(mp)
#define mutex_unlock(mp)	pthread_mutex_unlock(mp)
#define mutex_destroy(x)	pthread_mutex_destroy(x)

#if defined(HAVE_SEMAPHORE_ATOMIC_COUNT)
#define mutex_is_locked(x) (atomic_read(&(x)->__data.__count) == 0)
#else
#define mutex_is_locked(x) ((x)->__data.__count == 0)
#endif

#define MUTEX_HELD(x)	mutex_is_locked(x)

extern int mutex_init(mutex_t *, int, void *);

#endif
