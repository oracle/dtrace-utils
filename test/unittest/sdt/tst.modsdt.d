/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option destructive

/*
 * Ensure that SDT probes in modules are supported.
 */
BEGIN
{
	i = 0;
	system("test/triggers/testprobe");
}

sdt:dt_test::sdt-test
{
	trace(i);
}

sdt:dt_test::sdt-test
/++i == 20/
{
	exit(0);
}

sdt:dt_test::sdt-test
/i > 20/
{
	exit(1);
}
