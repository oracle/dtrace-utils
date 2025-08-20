#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./200built-in-variables-probe.d
 *
 *  DESCRIPTION
 *    We can get a probe's name components when it fires.
 *    The fully qualified name is provider:module:function:name,
 *    which we get with the built-in variables probeprov,
 *    probemod, probefunc, and probename, respectively.
 */

syscall:::entry
{
	printf("%s:%s:%s:%s\n", probeprov, probemod, probefunc, probename);
	exit(0);
}
