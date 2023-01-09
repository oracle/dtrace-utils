/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Test to check that attempting to enable too many probes will fail.
 *
 * This test will fail if:
 *	1) We ever execute on a platform which is capable of programming 10
 *	'PAPI_tot_ins' events simultaneously (which no current platforms are
 *	capable of doing).
 *	2) The system under test does not define the 'PAPI_tot_ins' event.
 */

#pragma D option quiet

BEGIN
{
	exit(0);
}

cpc:::PAPI_tot_ins-all-10000,
cpc:::PAPI_tot_ins-all-10001,
cpc:::PAPI_tot_ins-all-10002,
cpc:::PAPI_tot_ins-all-10003,
cpc:::PAPI_tot_ins-all-10004,
cpc:::PAPI_tot_ins-all-10005,
cpc:::PAPI_tot_ins-all-10006,
cpc:::PAPI_tot_ins-all-10007,
cpc:::PAPI_tot_ins-all-10008,
cpc:::PAPI_tot_ins-all-10009
{
}
