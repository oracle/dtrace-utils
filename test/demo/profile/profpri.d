/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: $_pid */
/* @@xfail: needs trigger */

profile-1001
/pid == $1/
{
	@proc[execname] = lquantize(curlwpsinfo->pr_pri, 0, 100, 10);
}
