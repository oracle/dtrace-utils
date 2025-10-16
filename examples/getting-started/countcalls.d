/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    countcalls.d - count the open(), read(), and write() calls for 5 seconds
 *
 *  SYNOPSIS
 *    sudo dtrace -s countcalls.d
 *
 *  DESCRIPTION
 *    List and count the calls to write(), read(), and open() executed, while
 *    the script is running.  The script automatically stops after 5 seconds.
 *
 *  NOTES
 *    - This script uses the profile provider to stop the tracing after a
 *    certain amount of time.  This time can easily be adjusted by changing.
 *    the number and unit.
 *
 *    - An anonymous aggregation is used to store the results.  Like a named
 *    aggregation, it is automatically printed when the tracing terminates.
 */

/*
 *  Fires every 5 seconds.  Since exit() is called, the tracing terminates
 *  the first time this probe fires and the clause is executed.
 */
profile:::tick-5sec
{
  exit(0);
}

/*
 *  Create the key by concatenating the function name and a string.  An
 *  alternative is to only use probefunc as a key and print the string as
 *  part of a printa() in the END probe: printa("%s () calls\n",@);
 */
syscall::write:entry, syscall::read:entry, syscall::open:entry
{
  @[strjoin(probefunc,"() calls")] = count();
}
