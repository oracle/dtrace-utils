#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./171provider_io.d
 *
 *  DESCRIPTION
 *    The io provider can be used to study I/O operations.
 *    For example, here we print whenever an I/O operation
 *    is started over the course of ten seconds, reporting
 *    the device name and whether it is a read or write
 *    operation.  Consult the documentation for more
 *    information on the probes and their arguments.
 */

BEGIN
{
	printf("%10s %2s\n", "DEVICE", "RW");
}

io:::start
{
	printf("%10s %2s\n",
	    args[1]->dev_statname,
	    args[0]->b_flags & B_READ ? "R" : "W");
}

tick-10s
{
	exit(0);
}
