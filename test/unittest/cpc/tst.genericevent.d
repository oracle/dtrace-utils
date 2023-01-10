/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Test that we can successfully enable a probe using a generic event.
 * Currently, all platforms implement 'cpu_clock' so we'll use that.
 * Note that this test will fail if the system under test does not
 * implement that event.
 *
 * This test will fail if:
 *	1) The system under test does not define the 'cpu_clock' event.
 */

#pragma D option quiet

cpc:::cpu_clock-all-10000
{
	@[probename] = count();
}

tick-3s
{
	exit(0);
}
