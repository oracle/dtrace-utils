/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -F option enables entry/return matching output.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -FZ -xswitchrate=1s */
/* @@timeout: 15 */
/* @@trigger: readwholedir */

BEGIN
{
	i = 0;
	j = 0;
}

syscall::read:
/pid == $target/
{
	printf("syscall: %d\n", i++);
}

fbt:vmlinux:SyS_read:,
fbt:vmlinux:__arm64_sys_read:,
fbt:vmlinux:__x64_sys_read:
/pid == $target/
{
	printf("fbt: %d\n", j++);
}
