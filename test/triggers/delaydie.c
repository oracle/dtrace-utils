/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2016 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Exit after a short delay.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main (void)
{
	struct timespec t;
	char *delay;

	delay = getenv("delay");
	if (!delay) {
		fprintf(stderr, "Delay in ns needed in delay env var.\n");
		exit(1);
	}

	t.tv_sec = strtoll(delay, NULL, 10) / 1000000000;
	t.tv_nsec = strtoll(delay, NULL, 10) % 1000000000;
	errno = 0;
	do {
		if (nanosleep(&t, &t) == 0)
			exit(0);
	} while (errno == -EINTR);
	exit(0);
}
