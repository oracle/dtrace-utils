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
 * ASSERTION: FBT provider entry/return self->var (TLS) lookup verification.
 *
 * SECTION: FBT Provider/Probes
 */

#pragma D option quiet
#pragma D option statusrate=10ms

BEGIN
{
	me = pid;
	num_entry = 0;
	num_return = 0;
	fails = 0;
}

fbt::SyS_ioctl:entry
/me == pid/
{
	num_entry++;
	self->token = pid;
}

fbt::SyS_ioctl:return
/me == pid && num_entry > 0/
{
	num_return++;
}

fbt::SyS_ioctl:return
/me == pid && num_entry > 0 && self->token != pid/
{
	fails++;
}

fbt::SyS_ioctl:entry,
fbt::SyS_ioctl:return
/num_entry >= 10 && num_return >= 10/
{
	exit(fails ? 1 : 0);
}
