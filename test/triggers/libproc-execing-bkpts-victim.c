/*
 * exec victim.
 *
 * This process exec()s constantly: between each exec() it randomly (50% of the
 * time) elects to invoke a function on which a breakpoint has been dropped.
 */

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/*
 * Copyright 2014 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

void __attribute__((__noinline__)) breakdance(void)
{
	asm ("");
}

int main (int argc, char *argv[])
{
	int snum = 0;
	char *snarg;
	char *exe;

	if (argc > 1)
		snum = atoi(argv[1]);

	if (snum > 5000)
		return 0;

	srand(snum + time(NULL));
	if (rand() < (RAND_MAX / 2))
		breakdance();

	asprintf(&snarg, "%i", ++snum);
	asprintf(&exe, "/proc/%i/exe", getpid());
	execl(exe, argv[0], snarg, (char *) NULL);
	perror("Cannot self-exec: should never happen\n");
	return 1;
}
