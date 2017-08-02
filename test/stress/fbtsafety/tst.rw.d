/*
 * Oracle Linux DTrace.
 * Copyright © 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option bufsize=1000
#pragma D option bufpolicy=ring
#pragma D option statusrate=10ms

fbt:::
{
	on = (timestamp / 1000000000) & 1;
}

fbt:::
/on/
{
	@[rw_read_held((rwlock_t *)&`max_pfn)] = count();
	@[rw_read_held((rwlock_t *)rand())] = count();
}

fbt:::
/on/
{
	@[rw_write_held((rwlock_t *)&`max_pfn)] = count();
	@[rw_write_held((rwlock_t *)rand())] = count();
}

fbt:::
/on/
{
	@[rw_iswriter((rwlock_t *)&`max_pfn)] = count();
	@[rw_iswriter((rwlock_t *)rand())] = count();
}

tick-1sec
/n++ == 20/
{
	exit(0);
}

