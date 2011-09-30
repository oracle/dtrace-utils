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

#ifndef _PORT_H
#define _PORT_H

#include <pthread.h>
#include <mutex.h>
#include <sys/processor.h>
#include <sys/types.h>

extern size_t strlcpy(char *, const char *, size_t);
extern size_t strlcat(char *, const char *, size_t);

extern int gmatch(const char *s, const char *p);

taskid_t gettaskid(void);
projid_t getprojid(void);
hrtime_t gethrtime(void);

int p_online (processorid_t cpun, int new_status);

int pthread_cond_reltimedwait_np(pthread_cond_t *cvp, pthread_mutex_t *mp,
    struct timespec *reltime);

int mutex_init(mutex_t *m, int flags1, void *ptr);

#endif
