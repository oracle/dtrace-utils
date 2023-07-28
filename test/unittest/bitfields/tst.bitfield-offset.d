/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Verify that DTrace is correctly determining the offset of a bitfield.  It
 * used to only look at ctm_offset to know the location of the bitfield in the
 * parent type, but some compiler/libctf combinations use ctm_offset for the
 * offset of the underlying type and depend on cte_offset instead to provide
 * the offset within that underlying type.
 */
#pragma D option quiet

struct iphdr	iph;

BEGIN
{
	iph.ihl = 5;
	iph.version = 4;

	trace(iph.ihl);
	trace(iph.version);

	exit(iph.ihl == 5 && iph.version == 4 ? 0 : 1);
}

ERROR
{
	exit(1);
}
