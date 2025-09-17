/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./200built-in-variables-probe.d
 *
 *  DESCRIPTION
 *    We can get a probe's name components when it fires.
 *    The fully qualified name is provider:module:function:name,
 *    which we get with the built-in variables probeprov,
 *    probemod, probefunc, and probename, respectively.
 */

syscall:::entry
{
	printf("%s:%s:%s:%s\n", probeprov, probemod, probefunc, probename);
	exit(0);
}
