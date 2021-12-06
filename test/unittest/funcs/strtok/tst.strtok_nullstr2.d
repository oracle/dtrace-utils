/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

BEGIN
{
	trace(strtok("abc", "def"));
}

BEGIN
{
	trace(strtok(NULL, "def"));
}

BEGIN
{
	exit(1);
}

ERROR
{
	exit(0);
}
