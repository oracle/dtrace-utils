/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

lockstat:::adaptive-acquire
/execname == "date"/
{
	@locks["adaptive"] = count();
}

lockstat:::spin-acquire
/execname == "date"/
{
	@locks["spin"] = count();
}
