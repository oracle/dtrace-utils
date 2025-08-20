#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./120provider_proc.d
 *
 *  DESCRIPTION
 *    We can track when processes throughout their life cycle.
 */

proc:::create
{
	printf("%s %s\n", probename, args[0]->pr_fname);
}

proc:::exec
{
	printf("%s %s\n", probename, args[0]);
}

proc:::start
{
	printf("%s %s\n", probename, execname);
}

proc:::exit
{
	printf("%s %s\n", probename, execname);
}

/*
 *  A tick probe is used to fire once to stop data collection.
 */
profile:::tick-10sec
{
	exit(0);
}
