#!/usr/sbin/dtrace -s

/*
 *  NAME
 *    fcalls.d - list the functions executed by an application
 *
 *  SYNOPSIS
 *    sudo ./fcalls.d -c "<name-of-app> [app options]"
 *
 *  DESCRIPTION
 *    This script lists the functions executed by the target
 *    application.  In addition to this, the number of calls to
 *    each function is printed.  This information is given on a
 *    per-thread basis, as well as aggregated over all threads.
 *
 *  NOTES
 *    - Since a.out is used in the probe definitions, library calls
 *    are excluded.  If a library like libc should be included,
 *    duplicate the probe definitions and in the copied lines,
 *    replace a.out by libc.so.
 *    For example:
 *      pid$target:a.out::entry,
 *      pid$target:libc.so::entry
 *        { <statements> }
 *
 *    - It is assumed that a function called main is executed.
 *    If this is not the case, this is not a critical error.
 *    The first probe is used to capture the name of the executable,
 *    but this is not essential.  The probe and printf() statement
 *    can safely be removed, or replaced by a suitable alternative.
 */

/*
 *  Suppress the default output from the dtrace command and have
 *  printa() print the aggregation data sorted by the first field.
 */
#pragma D option quiet
#pragma D option aggsortkey=1
#pragma D option aggsortkeypos=0

/*
 *  Store the name of the target application.  The probe is restricted
 *  to main only, because the exec name needs to be captured only once.
 */
pid$target:a.out:main:entry
{
  executable_name = execname;
}

/*
 *  Use 4 aggregations to store the total number of function calls, the
 *  counts per function and per thread, both seperately and differentiated
 *  by thread and function.
 */
pid$target:a.out::entry
{
  @total_call_counts                          = count();
  @call_counts_per_function[probefunc]        = count();
  @call_counts_per_thr[tid]                   = count();
  @counts_per_thr_and_function[tid,probefunc] = count();
}

/*
 *  Print the results.  Use format strings to create a table lay-out.
 */
END {
  printf("===========================================\n");
  printf("      Function Call Count Statistics\n");
  printf("===========================================\n");
  printf("Name of the executable: %s\n" ,executable_name);
  printa("Total function calls  : %@d\n",@total_call_counts);

  printf("\n===========================================\n");
  printf("      Aggregated Function Call Counts\n");
  printf("===========================================\n");
  printf("%-25s %12s\n\n","Function name","Count");
  printa("%-25s %@12d\n",@call_counts_per_function);

  printf("\n===========================================\n");
  printf("      Function Call Counts Per Thread\n");
  printf("===========================================\n");
  printf("%-7s %12s\n\n", "TID","Count");
  printa("%-7d %@12d\n",@call_counts_per_thr);

  printf("\n===========================================\n");
  printf("      Thread Level Function Call Counts\n");
  printf("===========================================\n");
  printf("%-7s  %-25s %8s\n\n","TID","Function name","Count");
  printa("%-7d  %-25s %@8d\n",@counts_per_thr_and_function);
}
