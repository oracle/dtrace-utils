#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./110provider_profile.d
 *
 *  DESCRIPTION
 *    The profile provider has basically two kinds of probes:
 *      - profile probes fire on all CPUs each time period
 *      - tick probes fire only once each time period
 *    One can specify either a frequency or a time period.
 */

/*
 *  A profile probe is used with higher frequency to sample
 *  what is executing on the system.
 */
profile:::profile-10hz
{
	printf("CPU %d: running %s\n", cpu, execname);
}

/*
 *  A tick probe is used to fire once to stop data collection.
 */
profile:::tick-2sec
{
	exit(0);
}
