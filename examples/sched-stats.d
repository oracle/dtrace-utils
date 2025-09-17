/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  NAME
 *    sched-stats.d - display various scheduler statistics
 *
 *  SYNOPSIS
 *    sudo ./sched-stats.d -c "<name-of-app> [app options]" <threshold>
 *
 *  DESCRIPTION
 *    This script shows several thread-scheduling statistics that
 *    were collected while the target application, or command, was
 *    running.
 *
 *    For good performance, a thread should run on the same CPU,
 *    or at least in the same NUMA node for as long as possible.
 *    In case it is taken off the CPU, it should be scheduled
 *    back on the same CPU, or at least in the same NUMA node.
 *
 *    The information printed, shows the scheduling statistics
 *    for any (multithreaded) application:
 *
 *    - For each thread, a histogram of the on-cpu times is shown.
 *
 *    - This is followed by a table with the overall timing statistics.
 *    For each thread, the total on-cpu time is given, plus the
 *    minimum and maximum on-cpu time.
 *
 *    - The script takes one parameter.  This is a threshold value.
 *    It is a lower limit (in microseconds) on the on-cpu time.
 *    For each thread it is reported how often the on-cpu time was
 *    at, or below, this value.  This table might point at threads
 *    that are falling behind on a (heavily loaded) system.
 *
 *    - The last 2 tables give information on the thread affinity.
 *    The first table of these 2 is sorted by thread ID and shows the
 *    CPU(s) used by each thread, as well how often the thread was
 *    scheduled on the CPU, and taken off again.
 *    The second table of these 2 shows the same information, but now
*     from a CPU centric view.
 *
 *  NOTES
 *    This script works with any target application or command.
 */

/*
 *  Suppress the default output from the dtrace command and have
 *  printa() print the aggregation data sorted by the first field.
 */

#pragma D option quiet
#pragma D option aggsortkey=1
#pragma D option aggsortkeypos=0

/*
 *  Place the argument from the command line in a global variable
 *  for better readability.
 */
BEGIN
{
  time_threshold = $1;
}

/*
 *  An on-cpu probe with a predicate to ensure that the probe only
 *  fires when the target is scheduled on the CPU.
 */
sched:::on-cpu
/ pid == $target /
{
/*
 *  Set the start value of the timer.  Since each thread needs
 *  to have its own timer, we use a thread-local variable.
 */
  self->ts_start = timestamp;

/*
 *  There are 2 aggregations.  Both increment their counter each
 *  time that this probe fires.  The thread ID (in tid) and CPU ID
 *  (in curcpu->cpu_id) are used to define the key.
 *
 *  The difference between the 2 aggregations is that the fields in
 *  the key have been swapped.  Given that printa() has been configured
 *  to sort the data by the first column, the first aggregation shows a
 *  thread centric view, while the second one has a CPU ID centric view.
 */
  @thr_cpu_id_on_cpu[tid,curcpu->cpu_id] = count();
  @cpu_id_thr_on_cpu[curcpu->cpu_id,tid] = count();
}

/*
 *  This is the first of 2 probes using off-cpu.  Again a predicate is
 *  used to restrict the data to the target command.
 */
sched:::off-cpu
/ pid == $target /
{
/*
 *  The process/thread is about to go off CPU.  Now is the time to
 *  record how long the thread has been running on the CPU.
 *  The value is converted from nanoseconds to microseconds (us) and
 *  stored in a clause-local variable.
 */
  this->time_on_cpu = (timestamp - self->ts_start)/1000;

/*
 *  This variable is used to update 4 aggregations: the histogram of
 *  the on-CPU times, the total on CPU time, plus the minimum and
 *  maximum time on the CPU.
 */
  @timings[tid]    = quantize(this->time_on_cpu);
  @total_time[tid] = sum(this->time_on_cpu);
  @min_time[tid]   = min(this->time_on_cpu);
  @max_time[tid]   = max(this->time_on_cpu);

/*
 *  These 2 aggregations are very similar to those used in the
 *  on-cpu probe.
 */
  @thr_cpu_id_off_cpu[tid,curcpu->cpu_id] = count();
  @cpu_id_thr_off_cpu[curcpu->cpu_id,tid] = count();

/*
 *  Free the storage for the thread-local variable since it is no
 *  longer needed.
 */
  self->ts_start = 0;
}

/*
 *  This is the second off-cpu probe.  It has a different and somewhat
 *  more complicated predicate.
 *  The clause is only executed for the target command and if the time
 *  on the CPU is below the user controllable threshold value.
 *  We basically record and count the event, but of course any other
 *  action can be taken here.
 *  An aggregation is used to store the data.  The thread ID (tid)
 *  is used as a key and we increment the counter.  This produces
 *  a count for each thread.
 */
sched:::off-cpu
/ pid == $target && this->time_on_cpu <= time_threshold /
{
  @thr_below[tid] = count();
}

/*
 *  This is where all results are printed.  It is all quite
 *  straightforward, but note that in three cases, multiple
 *  aggregations are printed with a single printa().  As long as the
 *  keys are the same, this is supported and makes it possible to
 *  print multiple columns with values.
 *
 *  Note that we do not need to include these print statements,
 *  because aggregations that are not explictly printed, are
 *  automatically printed when the script terminates.  The reason
 *  we print them ourselves is to have control over the lay-out.
 *  Another thing we do is to print more than one aggregation
 *  with a single printa() statement.
 */
END
{
  printf("Thread times on the CPU (us)\n");
  printa(@timings);

  printf("\n=====================================\n");
  printf("%10s %26s\n","TID", "Times on the CPU (us)");
  printf("%21s %6s %8s\n","Total","Min","Max");
  printf("=====================================\n");
  printa("%10d %@10d %@6d %@8d\n",@total_time,@min_time,@max_time);

  printf("\n=====================================\n");
  printf("Thread ON CPU time threshold: %d us\n",time_threshold);
  printf("=====================================\n");
  printf("%8s %26s\n","TID","Count below threshold");
  printa("%8d %@14d\n",@thr_below);

  printf("\n=====================================\n");
  printf("%10s  %8s %11s\n","TID","CPU ID","Count");
  printf("%37s\n","On CPU  Off CPU");
  printf("=====================================\n");
  printa("%10d  %8d %@7d %@8d\n",@thr_cpu_id_on_cpu,
                                 @thr_cpu_id_off_cpu);

  printf("\n=====================================\n");
  printf("%8s  %10s %11s\n","CPU ID","TID","Count");
  printf("%37s\n","On CPU  Off CPU");
  printf("=====================================\n");
  printa("%8d  %10d %@7d %@8d\n",@cpu_id_thr_on_cpu,
                                 @cpu_id_thr_off_cpu);
}
