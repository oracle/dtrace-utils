/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

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
