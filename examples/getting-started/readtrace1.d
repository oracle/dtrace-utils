/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    readtrace.d - show the time spent in the read() system call
 *
 *  SYNOPSIS
 *    sudo dtrace -s readtrace.d
 *
 *  DESCRIPTION
 *    For each combination of executable name and process id, show the
 *    total time in microseconds that is spent in the read() system call(s)
 *    executed by the df command.
 *
 *  NOTES
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - An aggregation is used to accumulate the timings.  An alternative
 *    is to print the results in the read:return probe and if required,
 *    post process the output when the script has completed.
 *
 *    - Although the results of an aggregation are automatically
 *    printed when the tracing terminates, in this case, the results
 *    are printed in the END probe.  The format string is optional,
 *    but is used to produce a table lay-out.
 */

/*
 *  Set the base value of the timer.  This is used as an offset in the
 *  read:return probe to calculate the time spent.
 *
 *  A predicate is used to select the df command.  All other commands
 *  skip the clause and do not set ts_base.
 */
syscall::read:entry
/ execname == "df" /
{
  self->ts_base = timestamp;
}

/*
 *  The predicate ensures that the base timing has been set.  Since this
 *  is only done for the df command, no information is collected for the
 *  other processes.
 */
syscall::read:return
/self->ts_base != 0/
{
/*
 *  Clause-local variable time_read is used to store the time passed
 *  since the read:entry probe fired.  This time is converted from
 *  nanoseconds to microseconds.
 */
  this->time_read = (timestamp - self->ts_base)/1000;
  @totals[execname,pid] = sum(this->time_read);

/*
 *  Free the storage for ts_base.
 */
  self->ts_base = 0;
}

/*
 *  Print the results.  The format is tailored to the df command.
 */
END
{
  printa("%-3s (pid=%-7d) spent a total of %5@d microseconds in read()\n",
         @totals);
}
