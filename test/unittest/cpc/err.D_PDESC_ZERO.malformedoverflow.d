/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Tests that specifying an overflow value containing extraneous characters
 * (only digits are allowed) will fail.
 */

BEGIN
{
	exit(0);
}

cpc:::PAPI_tot_ins-all-10000bonehead
{
	@[probename] = count();
}
