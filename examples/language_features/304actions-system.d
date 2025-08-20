#!/usr/sbin/dtrace -s

#pragma D option quiet
#pragma D option destructive

/*
 *  SYNOPSIS
 *    sudo ./304actions-system.d
 *
 *  DESCRIPTION
 *    One can launch system calls from within D script,
 *    but this must be explicitly allowed with the "destructive"
 *    option or dtrace -w switch.
 */

BEGIN
{
	system("ls");
	system("echo hello world");

	exit(0);
}
