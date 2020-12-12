/*
 * exec victim.
 *
 * This process exec()s constantly: between each exec() it randomly (50% of the
 * time) elects to invoke a function on which a breakpoint has been dropped.
 */

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2014, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

void __attribute__((__noinline__)) breakdance(void)
{
	asm ("");
}

int main (int argc, char *argv[])
{
	int snum = 0, total = 5000;
	char *snarg;
	char *exe;

	if (argc > 1)
		total = atoi(argv[1]);

	if (argc > 2)
		snum = atoi(argv[2]);

	if (snum > total)
		return 0;

	srand(snum + time(NULL));
	if (rand() < (RAND_MAX / 2))
		breakdance();

	asprintf(&snarg, "%i", ++snum);
	asprintf(&exe, "/proc/%i/exe", getpid());
	execl(exe, argv[0], argv[1], snarg, (char *)NULL);
	perror("Cannot self-exec: should never happen\n");
	return 1;
}
