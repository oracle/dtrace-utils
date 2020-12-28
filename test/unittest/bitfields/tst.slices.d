/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Check that lookup of types in CTF that happen to be bitfields works.
 * (These types are represented as slices.)
 * We are only interested in whether they parse, so exit immediately
 * after that.
 */

/* @@runtest-opts: -e */

#pragma D option quiet

syscall::ioctl:entry
{
	trace(curthread->sched_reset_on_fork);
}

fbt::tcp_parse_options:entry
{
	/* dtv2: use args[2] later */
	trace(((struct sk_buff *)arg2)->nohdr);
}
