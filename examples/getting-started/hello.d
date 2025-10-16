/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    hello.d - demonstrate the BEGIN probe
 *
 *  SYNOPSIS
 *    sudo dtrace -s hello.d
 *
 *  DESCRIPTION
 *    Demonstrate the use of the BEGIN probe.  Function trace() is
 *    used to print a string.  The exit() function terminates the
 *    tracing.
 */

/*
 *  The BEGIN probe fires when the tracing script starts.
 */
BEGIN
{
  trace("Hello, world");
  exit(0);
}
