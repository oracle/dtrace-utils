/*
 * Oracle Linux DTrace.
 * Copyright © 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: not yet ported */

fbt::delay:entry,
fbt::drv_usecwait:entry
{
	self->in = timestamp
}

fbt::delay:return,
fbt::drv_usecwait:return
/self->in/
{
	@snoozers[stack()] = quantize(timestamp - self->in);
	self->in = 0;
}
