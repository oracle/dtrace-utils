#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./180provider_cpc.d
 *
 *  DESCRIPTION
 *    The cpc (CPU Performance Counter) provider can be used
 *    profile activity based on hardware performance counters.
 *
 *    For a list of cpc probes, use "dtrace -lP cpc".
 *
 *    Probe specifications are of the form:
 *
 *        cpc:::<event name>-<mode>-<count>
 *
 *    The "event name" tells you which counters are available
 *    on your hardware.
 *
 *    The "mode" can be "kernel", "user", or "all", even if
 *    only "all" is listed.
 *
 *    The "count" indicates how many times the event is seen
 *    before the probe fires.  If the listed "count" is not
 *    suitable for your purposes, you can specify a different
 *    value.  It is usually best to start with a higher value,
 *    decreasing it if there are not enough probe firings,
 *    rather than starting too low and inundating your system
 *    with very many probe firings.
 *
 *    Here, an event perf_count_sw_cpu_clock-all-1000000000
 *    was listed by "dtrace -lP cpc".  We tweak the "mode"
 *    and "count" for our purposes.  The probe will fire every
 *    300 usecs of kernel activity.  We use aggregations to
 *    count how many times each call stack is seen.  The
 *    aggregation will be printed by default when the D
 *    script exits, after 10 seconds.
 *
 *    Consult the documentation for further information about
 *    cpc probes.
 */

cpc:::perf_count_sw_cpu_clock-kernel-300000000
{
	@[stack()] = count();
}

/* run for only 10 seconds */
tick-10s
{
	exit(0);
}
