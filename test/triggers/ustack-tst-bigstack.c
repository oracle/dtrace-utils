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
 * Copyright 2007 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

int limit = 4096;

int grow1(int);

int
shouldGrow(int frame)
{
	return frame >= limit-- ? 0 : 1;
}

int
grow(int frame)
{
	/*
	 * Create a ridiculously large stack - enough to push us over
	 * the default setting of 'dtrace_ustackdepth_max' (2048).
	 *
	 * This loop used to repeatedly call getpid(), but on Linux the result
	 * of that call gets cached, so that repeated calls actually do not
	 * trigger a system call anymore.  We use ioctl() instead.
	 */
	if (shouldGrow(frame))
		frame = grow1(frame++);

	for (;;)
		ioctl(-1, -1, NULL);

	grow1(frame);
}

int
grow1(int frame)
{
	if (shouldGrow(frame))
		frame = grow(frame++);

	for (;;)
		ioctl(-2, -2, NULL);

	grow(frame);
}

int
main(int argc, char *argv[])
{
	grow(1);

	return (0);
}
