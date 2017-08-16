/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: not yet ported */

fbt:ufs::entry
/arg1 == EIO/
{
	printf("%s+%x returned EIO.", probefunc, arg0);
}
