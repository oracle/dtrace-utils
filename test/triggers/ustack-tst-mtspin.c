/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

volatile long long count = 0;

int
baz(int a)
{
	(void) getpid();
	while (count != -1) {
		count++;
		a++;
	}

	return (a + 1);
}

int
bar(int a)
{
	return (baz(a + 1) - 1);
}

struct arg {
	int a;
	long b;
};

void *
foo(void *a)
{
	struct arg *arg = a;
	int *ret = malloc(sizeof(int));

	*ret = bar(arg->a) - arg->b;
	return ret;
}

int
main(int argc, char **argv)
{
	pthread_attr_t a;
	struct arg arg = { argc, (long)argv };
	pthread_t threads[2];
	int *one, *two;

	/* bleah. */
	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);

	pthread_create(&threads[0], &a, foo, &arg);
	pthread_create(&threads[1], &a, foo, &arg);

	pthread_attr_destroy(&a);
	pause();
	pthread_join(threads[0], (void **) &one);
	pthread_join(threads[1], (void **) &two);
	return *one - *two;
}
