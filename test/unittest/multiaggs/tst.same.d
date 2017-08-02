/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	@ = sum(90904);
	printa("%@d %@d %@d\n", @, @, @);
	printa("%@d %@d %@d\n", @);
	exit(0);
}
