/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The pcap() subroutine requires 2 arguments.
 *
 * SECTION: Actions and Subroutines/pcap()
 */

BEGIN
{
	pcap(0);
	exit(0);
}

ERROR
{
	exit(0);
}
