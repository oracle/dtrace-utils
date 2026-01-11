/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: rawfbt probes can be found even when a wildcard provider
 * description also allows fbt probes.
 */

/*
 * We expect to find 5 probes:
 *   BEGIN
 *   fbt vmlinux do_sys_openat2 entry
 *   fbt vmlinux do_sys_open entry
 *   rawfbt vmlinux do_sys_openat2 entry
 *   rawfbt vmlinux do_sys_open entry
 *
 * We want to validate:
 * - the FBT and rawfbt probes are all found
 * - the script executes (dtrace could attach to the probes)
 */

BEGIN,
*fbt:vmlinux:do_sys_open*:entry
{
	printf("success\n");
	exit(0);
}
