/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Ensure BPF code generated for an FBT return probe passes the BPF
 *	      verifiier.
 *
 * SECTION: FBT Provider/Probe arguments
 */

dtrace:::BEGIN,
fbt:vmlinux:adxl_get_component_names:return
{
	exit(0);
}

dtrace:::ERROR 
{
	exit(1);
}
