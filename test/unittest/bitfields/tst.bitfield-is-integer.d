/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Bitfields are treated as integers.
 *
 * SECTION: Types, Operators, and Expressions/Bitwise Operators
 */

#pragma D option quiet

/* @@runtest-opts: -e */

/* @@trigger: bogus-ioctl */

syscall::ioctl:entry / curthread->frozen == 0 /
{ exit(0); }

syscall::ioctl:entry / curthread->frozen != 0 /
{ exit(0); }
