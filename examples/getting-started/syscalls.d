/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    syscalls.d - show the read() system calls executed
 *
 *  SYNOPSIS
 *    sudo dtrace -s syscalls.d
 *
 *  DESCRIPTION
 *    Count the read() system calls that are executed while the script
 *    is running.  Count by the name of the process and the file
 *    descriptor used in the read operation.
 *
 *  NOTES
 *    - This script traces the running processes and the probe fires
 *    if there are calls to read().  If there are no such calls, no
 *    output is produced.
 *
 *    If that is the case, you can always execute a command that
 *    executes calls to read().  One such command is "date".  It causes
 *    the probe to fire, but any other command that issues calls to
 *    read() will do.
 *
 *    - Execute this script in the background, and type in the command,
 *    or run it in the foreground and type in the command in a separate
 *    terminal window on the same system.
 *
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - The results of the aggregation are automatically printed when
 *    the tracing terminates.
 */

/*
 *  The file descriptor used in the read() call is passed to the syscall
 *  and to the D probe in arg0.
 */
syscall::read:entry
{
  @totals[execname,arg0] = count();
}
