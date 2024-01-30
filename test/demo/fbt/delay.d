/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: none */

fbt::do_nanosleep:entry
{
	self->in = timestamp
}

fbt::do_nanosleep:return
/self->in/
{
	@snoozers[stack()] = quantize(timestamp - self->in);
	self->in = 0;
}
