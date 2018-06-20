/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void *
exec_thread(void *unused __attribute__((__unused__)))
{
	execlp("true", "true", NULL);
	perror("proc-tst-pthread-exec: exec() failed: "
	    "test results not meaningful");
	return NULL;
}

int
main(int argc, char **argv)
{
	pthread_t t;

	if (pthread_create(&t, NULL, exec_thread, NULL) < 0) {
		perror("proc-tst-pthread-exec: cannot create "
		    "exec()ing subthread");
		exit(1);
	}

	pthread_join(t, NULL);

	fprintf(stderr, "proc-tst-pthread-exec: joined thread returned: "
	    "this should never happen\n");
	return 1;
}
