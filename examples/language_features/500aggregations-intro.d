#!/usr/sbin/dtrace -s

#pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./500aggregations-intro.d
 *
 *  DESCRIPTION
 *    Aggregations can be used to collect statistical data on
 *    some value.  Results are printed by default when the
 *    script terminates.
 */

tick-20hz
{
	/* do not even bother naming the aggregation;  just "@" */
	@ = count();
}

tick-1sec
{
	exit(0);
}
