/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: not yet ported */

fbt::putnext:entry
{
	@calls[stringof(args[0]->q_qinfo->qi_minfo->mi_idname)] = count();
}
