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

sdt:dt_test::sdt-test {}
sdt:dt_test::sdt-test-arg1 { trace(args[0]); }
sdt:dt_test::sdt-test-arg2 { trace(args[0]); trace(args[1]); }
sdt:dt_test::sdt-test-arg3 { trace(args[0]); trace(args[1]); trace(args[2]); }
sdt:dt_test::sdt-test-arg4 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); }
sdt:dt_test::sdt-test-arg5 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); }
sdt:dt_test::sdt-test-arg6 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); }
sdt:dt_test::sdt-test-arg7 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); }
sdt:dt_test::sdt-test-arg8 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); }

syscall::exit_group:entry
/execname == "testprobe"/
{
        exit(0);
}
