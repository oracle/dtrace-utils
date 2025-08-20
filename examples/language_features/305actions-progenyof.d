#!/usr/sbin/dtrace -s

#pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./305actions-progenyof.d
 *
 *  DESCRIPTION
 *    One can test whether one is progeny of another process.
 */

BEGIN
{
	printf("I am%s progeny of pid 1234\n", progenyof(1234) ? "" : " not");
	printf("I am%s progeny of myself\n", progenyof(pid) ? "" : " not");
	printf("I am%s progeny of my parent\n", progenyof(ppid) ? "" : " not");

	exit(0);
}
