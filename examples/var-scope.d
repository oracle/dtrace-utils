#!/usr/sbin/dtrace -s

/*
 *  NAME
 *    var-scope.d - initialize, update and print variables
 *
 *  SYNOPSIS
 *    sudo ./var-scope.d -c <target>
 *
 *  DESCRIPTION
 *    This script demonstrates some of the scoping rules.  It
 *    shows the behaviour of global and clause-local variables.
 *    The target can be an application, or any command.
 *
 *  NOTES
 *    - Other than the BEGIN and END probe used here, there are
 *    2 more probes for the fopen() function, but you can use any
 *    probe you like, or no additional probes at all.  You are
 *    encouraged to experiment with this example and make changes.
 *
 *    - In case this example is used with the date command, you will
 *    see something like this:
 *
 *    $ sudo ./var-scope.d -c date
 *    In BEGIN
 *        A = 10 B = 20
 *        this->C = 60
 *    In fopen - return
 *        A = 11 B = 21
 *    <output from the date command>
 *    In END
 *        A = 11 B = 21
 *    $

 *    - Thread-local variables are not demonstrated here, but many
 *    of the other examples, like io-stats.d, make extensive use
 *    of them.  Please check one of those scripts if you would like
 *    to learn more how to use thread-local variables.
 *
 *    - Below, the workings of each probe is explained.  Since the
 *    results depend on the command that is traced, we base the
 *    explanations on the output from the example above.
 */

/*
 *  Suppress the default output from the dtrace command.
 */
#pragma D option quiet

/*
 *  This demonstrates how to declare a variable outside of a clause.
 *  Variable B is declared to be of type int64_t.  This is a global
 *  variable.  It is initialized to zero by default.
 */
int64_t B;

/*
 *  In this clause for the BEGIN probe, A is initialized to 10 and B
 *  is given a value of 20.  Both are global variables.  Variable C is
 *  a clause-local variable initialized to 30.
 */
BEGIN
{
  A = 10;
  B = 20;
  this->C = 30;
}

/*
 *  In this clause for the BEGIN probe, clause-local variable C is updated
 *  to a value of 60.  This demonstrates that clause-local variables are
 *  local to the set of clauses executed for a particular probe firing.
 *  This set is executed in the order they appear in the program.
 *
 *  Here, there are 2 clauses for the BEGIN probe.  The printed values
 *  in the example above confirm this update for C, as well as the initial
 *  values for A and B.
 */
BEGIN
{
  this->C += 30;
  printf("In %s\n",probename);
  printf("\tA = %d B = %d\n",A,B);
  printf("\tthis->C = %d\n",this->C);
}

/*
 *  The clause in this probe updates A.  Since this is a global variable,
 *  this change is visible to all probes that fire after this probe has fired.
 */
pid$target::fopen:entry
{
  A += 1;
}

/*
 *  The clause in this probe updates B and prints both A and B.  The
 *  probefunc (fopen) and probename (return) are printed first, followed by
 *  the values for A and B.
 *  We are in the return probe here, so we know that the entry probe has
 *  fired too.  This means that A has been updated as well.
 *  This is exactly what we see in the output above: A is 11 and B is 21,
 *  as it has just been updated.
 *  Note that C is not visible here, because it is local to the BEGIN probe.
 */
pid$target::fopen:return
{
  B += 1;
  printf("In %s - %s\n",probefunc,probename);
  printf("\tA = %d B = %d\n",A,B);
}

/*
 *  Both A and B are global variables and are visible in the END probe.  The
 *  final values are printed here.
 */
END
{
  printf("In %s\n",probename);
  printf("\tA = %d B = %d\n",A,B);
}
