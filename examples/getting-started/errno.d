/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    errno.d - list and count the system calls with a non-zero errno value
 *
 *  SYNOPSIS
 *    sudo dtrace -s errno.d
 *
 *  DESCRIPTION
 *    Trace every system call that returns a non-zero value in errno.
 *    Show the name of the function, the value of errno and how often
 *    this function returned a non-zero value for errno.
 *
 *  NOTES
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - The value of errno is available upon the return from the system call.
 *
 *    - To present the results in a compact form, we use an aggregation
 *    called syscalls.  Otherwise we may get several screens with the
 *    information.  Plus that we then can't easily count the functions.
 *
 *    - Although the results of an aggregation are automatically
 *    printed when the tracing terminates, in this case, we want to
 *    control the format of the output.  This is why the results are
 *    printed using printa() in the END probe
 */

/*
 *  Store the information in an aggregation called syscalls.
 *  Use the predicate to only allow non-zero errno values that are
 *  within the range for errno.
 */
syscall:::return
/ errno > 0 && errno <= ERANGE /
{
  @syscalls[probefunc,errno] = count();
}

/*
 *  The printf() line prints the header of the table to follow.
 */
END
{
  printf("%20s %5s %5s\n\n","syscall","errno","count");
  printa("%20s %5d %@5d\n",@syscalls);
}
