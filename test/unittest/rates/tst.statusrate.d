/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *	Positive test of statusrate option.
 *
 * SECTION: Options and Tunables/statusrate
 */

/*
 * Tests the statusrate option, by checking that the time delta between
 * exit() and END is at least as long as mandated by the statusrate.
 */

#pragma D option statusrate=10sec

inline uint64_t NANOSEC = 1000000000;

tick-1sec
/n++ > 5/
{
	exit(2);
	ts = timestamp;
}

END
/(this->delta = timestamp - ts) > 2 * NANOSEC/
{
	exit(0);
}

END
/this->delta <= 2 * NANOSEC/
{
	printf("delta between exit() and END (%u nanos) too small",
	    this->delta);
	exit(1);
}

END
/this->delta > 20 * NANOSEC/
{
	printf("delta between exit() and END (%u nanos) too large",
	    this->delta);
	exit(1);
}
