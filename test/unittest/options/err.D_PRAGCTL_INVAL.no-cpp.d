/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: With no -xcpp option the input file is not pre-processed.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}

#if 0
#endif
