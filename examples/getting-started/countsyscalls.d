/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    countsyscalls.d - count system calls invoked by a specific user
 *
 *  SYNOPSIS
 *    sudo dtrace -s countsyscalls.d <uid>
 *
 *  DESCRIPTION
 *    List and count all the system calls executed by the specified user id.
 *    The user id is passed in as an argument to the script.
 *
 *  NOTES
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - The results of the aggregation are automatically printed when
 *    the tracing terminates.
 */

/*
 *  Fires on every system call executed.  An aggregation called syscalls
 *  uses the function name as a key and counts the number of calls to this
 *  function.
 */
syscall:::entry
/pid == $1/
{
  @syscalls[probefunc] = count();
}
