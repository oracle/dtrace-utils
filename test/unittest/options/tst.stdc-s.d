/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -C -xstdc=s */

BEGIN
{
#ifdef __STDC__
	exit(1);
#else
	exit(0);
#endif
}
