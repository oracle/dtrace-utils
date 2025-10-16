/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    execcalls.d - show all processes that start executing
 *
 *  SYNOPSIS
 *    sudo dtrace -s execcalls.d
 *
 *  DESCRIPTION
 *    The probe in this script traces the exec() system call.  It
 *    fires whenever a process loads a new process image.
 *
 *  NOTES
 *    - This script traces the processes that start executing while
 *    the script is running.  If no process is started during the
 *    time that the script runs, no output is produced.
 *
 *    If that is the case, you can always execute a command yourself
 *    while this script is running.  One such command is "date" that
 *    causes the probe to fire.
 *
 *    - If you'd like to execute command(s) while the script is running,
 *    execute this script in the background, and type in one or more
 *    commands.  If you started the script in the foreground, type in
 *    the command(s) in a separate terminal window on the same system.
 *
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 */

proc:::exec
/ args[0] != NULL /
{
/*
 *  This information is from the DTrace user guide.  The proc:::exec
 *  probe makes a pointer to a char available in args[0].  This has
 *  the path to the new process image.
 *
 *  The strjoin() function is used to add a newline (\n) to the
 *  string that is to be printed.
 */
  trace(strjoin(stringof(args[0]),"\n"));
}
