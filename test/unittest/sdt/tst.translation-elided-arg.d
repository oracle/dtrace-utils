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
 * Copyright 2016 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* @@trigger: testprobe */

sdt:dt_test::sdt-test-ioctl-file {}

sdt:dt_test::sdt-test-ioctl-file
{
        /* This lies beyond an elided probe argument. */
	trace(args[1]->f_cred->uid.val);
}

syscall::exit_group:entry
/execname == "testprobe"/
{
        exit(0);
}
