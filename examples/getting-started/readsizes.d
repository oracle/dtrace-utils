/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    readsizes.d - show the distribution of bytes read when running find
 *
 *  SYNOPSIS
 *    sudo dtrace -s readsizes.d
 *
 *  DESCRIPTION
 *    Trace the calls to read() and use a predicate to only select those
 *    calls executed as part of executing the find command.  For such
 *    calls, show the distribution of the sizes.
 *
 *  NOTES
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - The results are stored in an aggregation called dist with
 *    the string "find" as the key.
 *
 *    - The results of an aggregation are automatically printed when
 *    the tracing terminates.
 */

/*
 *  A predicate is used to guarantee that the clause for the read:entry
 *  probe is only executed in case the call to read() was issued by the
 *  find command.
 *
 *  The quantize() function is used to show the distribution of the sizes
 *  read, or attempted to be read, by the read() call.  This value is
 *  passed to the syscall function and to the D probe in arg2.
 */
syscall::read:entry
/execname == "find"/
{
  @dist["find"] = quantize(arg2);
}
