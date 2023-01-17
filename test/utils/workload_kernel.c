/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>
#define BUFLEN 256

#if 0
/* use getrandom() wrapper in glibc */
#include <sys/random.h>
#else
/* getrandom() wrapper was not added to glibc until 2.25 */
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/random.h>
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
	return syscall(__NR_getrandom, buf, buflen, flags);
}
#endif

/*
 * The command-line argument specifies how many iterations to execute
 * in this kernel-intensive loop to run.
 */

int
main(int argc, const char **argv)
{
	char buf[BUFLEN];
	long long n;

	if (argc < 2)
		return 1;
	n = atoll(argv[1]);

	for (long long i = 0; i < n; i++)
		if (getrandom(buf, BUFLEN, GRND_NONBLOCK) != BUFLEN)
			return 1;

	return 0;
}
