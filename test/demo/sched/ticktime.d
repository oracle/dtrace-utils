/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

uint64_t last[int];

sched:::tick
/last[cpu]/
{
	@[cpu] = min(timestamp - last[cpu]);
}

sched:::tick
{
	last[cpu] = timestamp;
}
