/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -c /bin/true */

/*
 * This can hang due to premature cleanup of dprn notifications on
 * release.
 */

syscall:::entry
/pid == $target/
{
	ustack(1);
}
