/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: longsleep */
/* @@no-xfail */

/*
 * ASSERTION: can't specify a bogus library name
 *
 * SECTION: User Process Tracing/pid Provider
 *
 */

pid$target:libbmc_sucks.so.1::entry
{
}
