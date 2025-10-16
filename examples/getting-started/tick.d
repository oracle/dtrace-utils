/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    tick.d - perform an action at regular intervals
 *
 *  SYNOPSIS
 *    sudo dtrace -s tick.d
 *
 *  DESCRIPTION
 *    Use the tick probe from the profile provider to execute a block of code
 *    at regular intervals.  In this case, this is the update of a variable
 *    called "i", but the clause can contain any valid D statements.  The
 *    final value of "i" is printed in the END probe.
 *
 *  NOTES
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - Instead of printf(), trace() can be used and vice-versa.  The
 *    difference is that trace() does not support a format string.
 */

/*
 *  Initialize variable "i" to zero.  This is a global variable that
 *  can be read and written by any probe.
 */
BEGIN
{
  i = 0;
}

/*
 *  This probe fires every 100 milliseconds.  When it fires, it updates
 *  variable "i" and prints the result.
 */
profile:::tick-100msec
{
  printf("i = %d\n",++i);
}

/*
 *  Print the final result.
 */
END
{
  trace(i);
}
