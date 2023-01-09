/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Tests that attempting to enable a probe containing a non existent event
 * will fail.
 */

BEGIN
{
	exit(0);
}

cpc:::nonexistentevent-all-10000
{
	@[probename] = count();
}
