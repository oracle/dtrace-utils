/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -qC */

/*
 * Depending on how the kernel was configured, throttling can introduce
 * occasional large gaps between cpu_clock firings.  So, measure these
 * gaps.  None should be too low, but a few may be too high.
 */

#define PERIOD 50000000

long long lasttime[int];

cpc:::cpu_clock-all-PERIOD
{
	currtime[cpu] = timestamp;
}

cpc:::cpu_clock-all-PERIOD
/lasttime[cpu] != 0/
{
	this->gap = currtime[cpu] - lasttime[cpu];
	@["number"] = sum(1);
	@["low" ] = sum(this->gap <  9 * (PERIOD / 10) ? 1 : 0);
	@["high"] = sum(this->gap > 15 * (PERIOD / 10) ? 1 : 0);
}

cpc:::cpu_clock-all-PERIOD
{
	lasttime[cpu] = currtime[cpu];
}

tick-5s
{
	exit(0);
}
