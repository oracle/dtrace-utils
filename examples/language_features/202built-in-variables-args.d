/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./202built-in-variables-args.d
 *
 *  DESCRIPTION
 *    A probe's arguments can be accessed through the built-in
 *    variables arg0, arg1, arg2, etc.  These are all signed,
 *    64-bit integers, whose meanings depend on the probe.
 *
 *    Some probes have typed arguments args[0], args[1], args[2],
 *    etc.  To see the types, use "dtrace -lvn $probe".  Their
 *    meanings are described in the documentation.
 *
 *    When arguments are typed, they may also be reordered.
 *    So there may be no correspondence between, say, arg0
 *    and args[0].
 *
 *    Typically, it is better to use the typed args[] when
 *    the probe has them.
 */

syscall::write:entry
{
	printf("Write %d bytes to fd %d\n", args[2], arg0);
	exit(0);
}
