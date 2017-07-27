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
 * Copyright 2011 -- 2017 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <config.h>				/* for HAVE_* */

#ifndef HAVE_WAITFD
#include <errno.h>
#include <unistd.h>				/* for syscall() */
#include <linux/version.h>                      /* for KERNEL_VERSION() */
#include <port.h>                               /* for linux_version_code() */
#include <dt_debug.h>

/*
 * Translates waitpid() into a pollable fd.
 *
 * The syscall number varies between kernel releases.
 * The version code in this table is the kernel version in which a particular
 * value was introduced (i.e. a lower bound).  Kernels with major/minor numbers
 * not in this list are considered unknown, and we return -ENOSYS.  A syscall
 * number of zero terminates the list.
 */

static struct waitfds_tag {
        unsigned long linux_version_code;
        long waitfd;
} waitfds[] = { { KERNEL_VERSION(4,12,0), 361 },
		{ KERNEL_VERSION(4,11,0), 361 },
		{ KERNEL_VERSION(4,10,0), 360 },
		{ KERNEL_VERSION(4,9,0), 360 },
		{ KERNEL_VERSION(4,8,0), 360 },
		{ KERNEL_VERSION(4,6,0), 360 },
		{ KERNEL_VERSION(4,5,0), 358 },
		{ KERNEL_VERSION(4,1,4), 351 },
		{ 0, 0 } };

static long waitfd_nr;

int
waitfd(int which, pid_t upid, int options, int flags)
{
        if (!waitfd_nr) {
                struct waitfds_tag *walk;
                unsigned long version = linux_version_code();

                for (walk = waitfds; walk->waitfd; walk++) {
                        if ((version >= walk->linux_version_code) &&
                            ((version >> 8) == (walk->linux_version_code >> 8))) {
                                waitfd_nr = walk->waitfd;
                                break;
                        }
                }
		if (!waitfd_nr) {
			dt_dprintf("waitfd() syscall number for this kernel "
			    "not known.\n");
			return -ENOSYS;
		}
        }

        return syscall(waitfd_nr, which, upid, options, flags);
}

#endif
