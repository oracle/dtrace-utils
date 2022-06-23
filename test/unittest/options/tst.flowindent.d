/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: flowindent is broken */

/*
 * ASSERTION: The -xflowindent option enables entry/return matching output.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xflowindent -Z */
/* @@timeout: 15 */
/* @@trigger: readwholedir */

BEGIN
{
	i = 0;
	j = 0;
	k = 0;
}

syscall::read:
/i < 10/
{
	printf("syscall: %d\n", i++);
}

fbt:vmlinux:SyS_read:,
fbt:vmlinux:__arm64_sys_read:,
fbt:vmlinux:__x64_sys_read:
/j < 10/
{
	printf("fbt: %d\n", j++);
}

profile:::tick-1sec
/k < 4/
{
	printf("profile: %d\n", k++);
}

profile:::tick-10sec
/i > 9 && j > 9 && k > 3/
{
	exit(0);
}
