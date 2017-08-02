/*
 * Oracle Linux DTrace.
 * Copyright © 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

/* @@xfail: fbt provider not yet implemented */

sdt:::callout-start
{
	self->callout = ((callout_t *)arg0)->c_func;
}

fbt::timeout:entry
/self->callout && arg2 <= 100/
{
	/*
	 * In this case, we are most interested in interval timeout(9F)s that
	 * are short.  We therefore do a linear quantization from 0 ticks to
	 * 100 ticks.  The system clock's frequency ? set by the variable
	 * "hz" ? defaults to 100, so 100 system clock ticks is one second. 
	 */
	@callout[self->callout] = lquantize(arg2, 0, 100);
}

sdt:::callout-end
{
	self->callout = NULL;
}

END
{
	printa("%a\n%@d\n\n", @callout);
}
