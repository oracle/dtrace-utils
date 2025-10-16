/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    cswpercpu.d - print the number of context switches per CPU per second
 *
 *  SYNOPSIS
 *    sudo dtrace -s cswpercpu.d
 *
 *  DESCRIPTION
 *    Every second, print the CPU id, the number of context switches each
 *    CPU performed, plus the total number of context switches executed
 *    across all the CPUs.  For each info block, include a time stamp.
 *
 *  NOTES
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - The results are stored in an aggregation called cswpersec.
 *    Every second, the results are printed with printa() and the
 *    aggregation is cleared.
 *
 *    - In addition to using the CPU ID as a key in the cswpersec
 *    aggregation, also the string "total" is used.  This entry
 *    is always printed last, because by default, printa() prints
 *    the results sorted by the value.  The total count for any of
 *    the CPU Ids is always equal or less than "total".
 */

/*
 *  To avoid that the carefully crafted output is mixed with the
 *  default output by the dtrace command, enable quiet mode.
 */
#pragma D option quiet

/*
 *  Print the header.
 */
BEGIN
{
  printf("%-20s %5s %15s", "Timestamp", "CPU", "#csw");
}

/*
 *  Fires when a process is scheduled to run on a CPU.
 */
sched:::on-cpu
{
/*
 *  Convert the CPU ID to a string.  This needs to be done because
 *  key "total" is a string.
 */
  this->cpustr = lltostr(cpu);
/*
 *  Update the count.
 */
  @cswpersec[this->cpustr]  = count();
  @cswpersec["total"] = count();
}

/*
 *  Fires every second.
 */
profile:::tick-1sec
{
/*
 *  Print the date and time first
 */
  printf("\n%-20Y ", walltimestamp);

/*
 *  Print the aggregated counts for each CPU and the total for all CPUs.
 *  Use some formatting magic to get a special table lay-out.
 */
  printa("%5s %@15d\n                     ", @cswpersec);

/*
 *  Reset the aggregation.
 */
  clear(@cswpersec);
}
