/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

sched:::tick,
sched:::enqueue
{
	@[probename] = lquantize((timestamp / 1000000) % 10, 0, 10);
}
