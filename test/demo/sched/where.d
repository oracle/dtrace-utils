/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

sched:::on-cpu
{
	self->ts = timestamp;
}

sched:::off-cpu
/self->ts/
{
	@[cpu] = quantize(timestamp - self->ts);
	self->ts = 0;
}
