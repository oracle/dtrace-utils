/*
 * Oracle Linux DTrace.
 * Copyright © 2008, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * It isn't permitted for a main program to contain a depends_on imperative.
 */

#pragma D depends_on library net.d

BEGIN
{
	exit(0);
}
