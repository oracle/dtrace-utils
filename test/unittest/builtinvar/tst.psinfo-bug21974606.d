/*
 * Oracle Linux DTrace.
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * To ensure pr_psargs does not have an (extra) trailing space.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("/bin/ls >/dev/null");
}

exec-success
/execname == "ls"/
{
	psargs = stringof(curpsinfo->pr_psargs);
	printf("psargs [%s]\n", psargs);
	printf("Last char is [%c]\n", psargs[strlen(psargs) - 1]);
	exit(psargs[strlen(psargs) - 1] == ' ' ? 1 : 0);
}
