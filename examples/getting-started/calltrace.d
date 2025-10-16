/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    calltrace.d - time all system calls for the cp command
 *
 *  SYNOPSIS
 *    sudo dtrace -s calltrace.d
 *
 *  DESCRIPTION
 *    List and time all the system calls that are executed during
 *    a cp command.
 *
 *  NOTES
 *    - This script traces all system calls that are executed when
 *    a cp command is run.
 *
 *    This means that you need to execute the cp command while the
 *    script is running.  There are two ways to do this:
 *    o Execute this script in the background, and type in the cp command.
 *    o Alternatively, run the script in the foreground and type
 *    the cp command in a separate terminal window on the same system.
 *
 *    - You can use any file to copy, but you can also generate a file
 *    and then copy it.  This is an example how to create a 500 MB file,
 *    copy it with the cp command and remove both files again:
 *    $ dd if=/dev/zero of=tmp_file bs=100M seek=5 count=0
 *    $ cp tmp_file tmp_file2
 *    $ rm tmp_file tmp_file2
 *
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - Although the results of an aggregation are automatically
 *    printed when the tracing terminates, in this case, we want to
 *    control the format of the output.  This is why the results are
 *    printed using printa() in the END probe
 */

/*
 *  Set the base value of the timer.  This is used as an offset in the
 *  return probe to calculate the time spent in a system call.
 *
 *  A predicate is used to select the cp command.  All other commands
 *  skip executing the clause and do not set ts_base.
 */
syscall:::entry
/ execname == "cp" /
{
  self->ts_base = timestamp;
}

/*
 *  The predicate ensures that the base timing has been set.
 *  Since this is only done for the cp command, no information
 *  is collected for the other processes.
 */
syscall:::return
/self->ts_base != 0/
{
/*
 *  Compute the time passed since the entry probe fired and
 *  convert the nanosecond value to microseconds.
 *
 *  Update the aggregation called totals with this time.  The
 *  execname (which is cp here) and the system call that caused
 *  the probe to fire, are the fields in the key.
 */
  this->time_call = (timestamp - self->ts_base)/1000;
  @totals[execname,probefunc] = sum(this->time_call);

/*
 *  Free the storage for ts_base.
 */
  self->ts_base = 0;
}

/*
 *  Print the results.  Use printf() to print a description of
 *  the contents of the aggregation.  The format string in printa()
 *  is used to create a table lay-out.
 */
END
{
  printf("System calls executed and their duration:\n");
  printa("%15s executed %18s - this took a total of %@8d microseconds\n",
         @totals);
}
