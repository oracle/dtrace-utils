/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: $_pid execute */
/* @@xfail: pid provider not yet implemented */

pid$1::$2:entry
{
	self->trace = 1;
}

pid$1::$2:return
/self->trace/
{
	self->trace = 0;
}

pid$1:::entry,
pid$1:::return
/self->trace/
{
}
