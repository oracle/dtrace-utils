/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    activity.d - report on process create, exec and exit
 *
 *  SYNOPSIS
 *    sudo dtrace -s activity.d
 *
 *  DESCRIPTION
 *    Show the processes that are created, executed and exited
 *    while the script is running.
 *
 *  NOTES
 *    - This script uses the proc provider to trace the following process
 *    activities: create, exec, and exit.
 *
 *    - This script is guaranteed to produce results if you start one or
 *    more commands while the script is running.  There are two ways
 *    to do this:
 *    o Execute this script in the background, and type in the command(s).
 *    o Alternatively, run the script in the foreground and type the
 *    command(s) in a separate terminal window on the same system.
 *
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - Associative arrays are used to store the information from the
 *    proc provider.
 *
 *    - The DTrace User Guide documents the proc provider probe
 *    arguments like args[0] and also structures like psinfo_t.  It is
 *    strongly recommended to check the documentation for this info.
 */

/*
 *  Fires when a process (or process thread) is created using fork() or
 *  vfork(), which both invoke clone().  The psinfo_t corresponding to
 *  the new child process is pointed to by args[0].
 */
proc:::create
{
/*
 *  Store the PID of both the parent and child process from the psinfo_t
 *  structure pointed to by args[0].  Use 3 associative arrays to store the
 *  various items of interest.
 */
  this->childpid  = args[0]->pr_pid;
  this->parentpid = args[0]->pr_ppid;

/*
 *  Store the parent PID of the new child process.
 */
  p_pid[this->childpid] = this->parentpid;

/*
 *  Parent command name.
 */
  p_name[this->childpid] = execname;

/*
 *  Child has not yet been exec'ed.
 */
  p_exec[this->childpid] = "";
}

/*
 *  The process starts.  In case proc:::create has fired, store the
 *  absolute time and the full name of the child process.
 */
proc:::exec
/ p_pid[pid] != 0 /
{
  time[pid]   = timestamp;
  p_exec[pid] = args[0];
}

/*
 *  The process starts, but in this case, proc:::create has not fired.
 *  In addition to storing the name of the child process, store the
 *  various other items of interest.
 */
proc:::exec
/ p_pid[pid] == 0 /
{
  time[pid]   = timestamp;
  p_exec[pid] = args[0];
  p_pid[pid]  = ppid;
  p_name[pid] = execname;
}

/*
 *  The process exits.  Print the information.
 */
proc:::exit
/p_pid[pid] != 0 && p_exec[pid] != ""/
{
  printf("%-16s (%d) executed %s (%d) for %d microseconds\n",
    p_name[pid], p_pid[pid], p_exec[pid], pid, (timestamp - time[pid])/1000);
}

/*
 *  The process has forked itself and exits.  Print the information.
 */
proc:::exit
/p_pid[pid] != 0 && p_exec[pid] == ""/
{
  printf("%-16s (%d) forked itself (as %d) for %d microseconds\n",
    p_name[pid], p_pid[pid], pid, (timestamp - time[pid])/1000);
}

/*
 *  Assign 0s to free memory associated with this pid.
 */
proc:::exit
/p_pid[pid] != 0/
{
  time[pid]   = 0;
  p_exec[pid] = NULL;
  p_pid[pid]  = 0;
  p_name[pid] = NULL;
}
