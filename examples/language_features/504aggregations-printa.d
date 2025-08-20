#!/usr/sbin/dtrace -s

# pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./504aggregations-printa.d
 *
 *  DESCRIPTION
 *    While aggregations are printed by default when the D
 *    script terminates, it can be useful to print aggregations
 *    explicitly using the printa() function -- either to use
 *    detailed formatting or if results should be printed
 *    while the script is still running.  In addition to the
 *    usual formatting directives used by printf(), printa()
 *    also uses "@" directives for the aggregation values.
 */

tick-20hz
{
	@["key"] = count();
}

tick-1sec
{
	printa("KEY BEFORE VALUE: %s %@d\n", @);
	printa("VALUE BEFORE KEY: %@d %s\n", @);
	exit(0);
}
