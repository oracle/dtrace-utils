/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Test to check that attempting to enable a valid event with a frequency
 * lower than the default platform limit will fail.
 *
 * This test will fail if:
 *	1) The system under test does not define the 'PAPI_tot_ins' event.
 *	2) The 'dcpc-min-overflow' variable in dcpc.conf has been modified.
 */

#pragma D option quiet

BEGIN
{
	exit(0);
}

cpc:::PAPI_tot_ins-all-100
{
}
