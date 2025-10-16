/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    daterun.d - display arguments to write() for the date command
 *
 *  SYNOPSIS
 *    sudo dtrace -s daterun.d
 *
 *  DESCRIPTION
 *    Trace the calls to write(), but only when executed by the date
 *    command.  For such calls, print the file descriptor, the output
 *    string, and the number of bytes printed.
 *
 *  NOTES
 *    - Execute this script in the background, and type in "date", or
 *    run it in the foreground and type in "date" in a separate window.
 *
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - The output string is stored in arg1 and contains a newline (\n)
 *    character.  This is why the byte count is printed on a separate line.
 */

syscall::write:entry
/execname == "date"/
{
/*
 *  Use copyinstr() to copy the string from user space into a DTrace
 *  buffer in kernel space.  This function returns a pointer to the buffer.
 */
  printf("%s (fd=%d output=%s bytes=%d)\n",probefunc, arg0,
                                           copyinstr(arg1), arg2);
}
