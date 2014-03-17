/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006, 2014 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
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
