#!/usr/sbin/dtrace -s
/*
 *  NAME
 *    sched-simple.d - count how often the target was on/off the CPU
 *
 *  SYNOPSIS
 *    sudo ./sched-simple.d -c <target>
 *
 *  DESCRIPTION
 *    This script uses the sched provider to count how often the
 *    target proces was scheduled on and off the CPU.  The target
 *    can be an application, or any command.
 *    The information is given for all individual threads used by
 *    the target, as well as across all the threads.
 *
 *  NOTES
 *    There are 2 aggregations.  The one with 2 fields in the
 *    key, thr_count_states, uses the thread ID as the first field.
 *    By configuring printa() through pragmas to print the data
 *    sorted by the first field, the results for this aggregation
 *    are shown on a per thread basis.
 */

/*
 *  Suppress the default output from the dtrace command and have
 *  printa() print the aggregation data sorted by the first field.
 */
#pragma D option quiet
#pragma D option aggsortkey=1
#pragma D option aggsortkeypos=0

/*
 *  These are 2 probes from the sched provider that share a clause.
 *  Without a predicate, these probes will fire as soon as any
 *  process goes on/off the CPU.  That will work fine, but in this
 *  case we use a predicate to restrict the probing to the target
 *  application, or command.
 *
 */
sched:::on-cpu,
sched:::off-cpu
/ pid == $target /
{
/*
 *  We know that the probe name is either on-cpu or off-cpu.  This
 *  is used to assign a different string to clause-local variable
 *  called state.  This variable is used in the key for the aggregations
 *  below.
 */
  this->state = (probename == "on-cpu") ?
                  "scheduled on" : "taken off";

/*
 *  The 2 aggregations below are used to count how often the probes fired.
 *  The difference is in the key.  The first aggregation, count_states,
 *  does not use the thread ID in the key.  It counts across all
 *  threads.  In contrast with this, the second aggregation,
 *  thr_count_states, differentiates the counts by the thread ID.
 */
  @count_states[this->state]         = count();
  @thr_count_states[tid,this->state] = count();
}

/*
 *  Print the results.  We use printf() to print a header.  For both
 *  aggregations, a formatted printa() statement is used.  The
 *  printf() statement in between the 2 printa() statements is used
 *  to separate the 2 blocks with results.
 *
 *  Note that we do not need to include these print statements,
 *  because aggregations that are not explictly printed, are
 *  automatically printed when the script terminates.  The reason
 *  we print them ourselves is to have control over the lay-out.
 */
END
{
  printf("%8s %-20s %6s\n","TID","State","Count");
  printf("====================================\n");
  printa("%8d %-12s the CPU %@6d\n",@thr_count_states);
  printf("\n");
  printa("Total count %12s: %@d\n",@count_states);
}
