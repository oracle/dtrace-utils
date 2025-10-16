/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    countprogs.d - count processes invoked by a specific user
 *
 *  SYNOPSIS
 *    sudo dtrace -s countprogs.d <uid>
 *
 *  DESCRIPTION
 *    List and count every processes that is started by the user, while
 *    this script runs.  The user id is passed in as an argument to the
 *    script.
 *
 *  NOTES
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - To ensure a process is started while the script is running,
 *    either execute this script in the background, and type in one
 *    or more commands, or run it in the foreground and type in the
 *    command(s) in a separate terminal window on the same system.
 *
 *    - The results of an aggregation are automatically printed when
 *    the tracing terminates.
 */

/*
 *  Fires on every process that starts execution.  An aggregation called
 *  proc_name uses the executable name as a key and counts the number of
 *  times this executable, or process, is started.
 */
proc:::exec
/uid == $1/
{
  @proc_name[execname] = count();
}
