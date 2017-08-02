/*
 * Oracle Linux DTrace.
 * Copyright © 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: needs porting */

sched:::change-pri
{
	@[stringof(args[0]->pr_clname)] =
	    lquantize(args[2] - args[0]->pr_pri, -50, 50, 5);
}
