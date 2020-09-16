/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */
/*
 * ASSERTION:
 * A clause cannot contain a commit() followed by an aggregating action.
 *
 * SECTION: Speculative Tracing/Committing a Speculation;
 */

BEGIN
{
	commit(1);
	@a["foo"] = count();
}
