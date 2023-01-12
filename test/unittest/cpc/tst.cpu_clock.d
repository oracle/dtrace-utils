/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@reinvoke-failure: 2 */

/*
 * This test is probably unstable due to possible kernel throttling issues.
 * But on my ARM OL9 VM, it looks particularly bad due to emitting 14
 * probes and then just stopping for a long time.
 */
/* @@tags: unstable */

#pragma D option quiet

/* count quickly (each 0.05 sec) */
cpc:::cpu_clock-all-50000000
/cpu == 0/
{
	n++;
}

/* we expect 40 counts after 2 secs */
/* (possibly, there could be far fewer due to kernel throttling) */
cpc:::cpu_clock-all-2000000000
/cpu == 0 && (n < 20 || n > 42)/
{
	printf("count is out of range: %d (expect 40)\n", n);
	exit(1);
}

cpc:::cpu_clock-all-2000000000
/cpu == 0/
{
	printf("count is in range: %d\n", n);
	exit(0);
}

/* or time out */
tick-5s
{
	printf("time out: count is %d\n", n);
	exit(1);
}
