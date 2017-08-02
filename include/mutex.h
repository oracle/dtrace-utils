/*
 * Oracle Linux DTrace.
 * Copyright © 2011, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
