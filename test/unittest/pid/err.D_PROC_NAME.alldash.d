/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: longsleep */
/* @@no-xfail */

/*
 * ASSERTION: Verify that you can't enable "all" '-' function probes
 *
 * SECTION: User Process Tracing/pid Provider
 *
 */

pid$target::-:
{
}
