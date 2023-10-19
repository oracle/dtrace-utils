/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Verify that we can access the first character of the Linux banner string
 * (stored in kernel global variable linux_banner) using a pointer dereference.
 * The string always begins with "Linux".
 */

#pragma D option quiet

BEGIN
{
	exit(*(char *)&`linux_banner == 'L' ? 0 : 1);
}

ERROR
{
	exit(1);
}
