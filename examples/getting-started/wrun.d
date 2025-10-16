/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    wrun.d - display arguments to write() for the w command
 *
 *  SYNOPSIS
 *    sudo dtrace -s wrun.d
 *
 *  DESCRIPTION
 *    Trace the calls to write(), but only when executed by the w
 *    command.  For such calls, print the file descriptor, the
 *    output string, and the number of bytes printed.
 *
 *  NOTES
 *    - Execute this script in the background, and type in "w", or
 *    run it in the foreground and type in "w" in a separate window.
 *
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - DTrace has a default limit of 256 bytes for strings.  In this
 *    example, the output string may be longer than this.  If so,
 *    either use the "-x strsize=<new-length>" command line option,
 *    or a "#pragma D option strsize=<new-length>" pragma in the
 *    script to increase the size.  The latter is shown below.
 */

#pragma D option strsize=512

/*
 *  Use a predicate to only execute the clause in case the w
 *  command causes the probe to fire.
 */
syscall::write:entry
/execname == "w"/
{
/*
 *  Use copyinstr() to copy the string from user space into a DTrace
 *  buffer in kernel space.  This function returns a pointer to the buffer.
 *  The string it points to, is null terminated.
 *  The third argument in the call to write() is the number of bytes
 *  to be printed.  This could be used as an optional second argument
 *  in copyinstr() so that only this many bytes are copied.
 */
  printf("%s(fd=%d\noutput=\n%s\nbytes=%d)\n",probefunc, arg0,
                                              copyinstr(arg1), arg2);
}
