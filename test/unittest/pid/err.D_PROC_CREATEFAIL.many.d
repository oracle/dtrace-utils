/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: longsleep */
/* @@timeout: 600 */
/* @@no-xfail */
/* @@skip: causes following tests to be unreliable */

pid$target:::
{
}

BEGIN
{
	exit(1);
}
