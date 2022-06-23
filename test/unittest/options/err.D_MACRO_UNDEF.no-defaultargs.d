/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: With no -xdefaultargs option, missing arguments cause an error.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

BEGIN
{
	exit($1 ? 1 : 0);
}

ERROR
{
	exit(0);
}
