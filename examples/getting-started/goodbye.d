/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    goodbye.d - demonstrate the END probe
 *
 *  SYNOPSIS
 *    sudo dtrace -s goodbye.d
 *
 *  DESCRIPTION
 *    Demonstrates the use of the END probe.  Function trace() is
 *    used to print a string.
 *
 *  NOTES
 *    - The advantage of trace() is that it is simple and does not
 *    require a format string.  If more control over the output is
 *    needed, printf() is a good alternative.
 *
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 */

/*
 *  The END probe fires once when the tracing has terminated.
 */
END
{
  trace("Goodbye");
}
