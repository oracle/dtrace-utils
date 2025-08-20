#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./501aggregations-names-keys.d
 *
 *  DESCRIPTION
 *    Aggregations can be named or use keys.
 */

tick-20hz
{
	@my_count[(timestamp % 2 == 0) ? "even" : "odd"] = count();
}

tick-1sec
{
	exit(0);
}
