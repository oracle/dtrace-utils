/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option version=1.0

/*
 * The following identifiers were added as D built-ins as of version 1.1.
 * Using these identifiers as user-specified variables should be illegal in
 * that and any later versions, but legal in earlier versions.
 */
int strstr;
int strchr;
int strrchr;
int strtok;
int substr;
int index;
int freopen;

BEGIN
{
	exit(0);
}

