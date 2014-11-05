/*
 * A process that madly dlopen()s and dlclose()s shared libraries in a number of
 * threads: some of them are in secondary namespaces.
 *
 * It also exec()s itself once.
 */

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
 * Copyright 2013, 2014 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <pthread.h>

static int load_count;
static int unload_count;
static long looptime = 10;
static int many_lmids;

static void *churn(void *unused)
{
	struct timeval start, now;
	Lmid_t lmids[6] = {0};
	void *loaded[6] = {0};
	void *non_dlm = NULL;
	char *error;

	gettimeofday(&start, NULL);

	/*
	 * Populate the dlmopen set.
	 *
	 * I'd like to continuously open and close these, but that is
	 * impossible: this causes a massive TLS space leak and failure to
	 * dlopen() after only a dozen or so opens.  So for now, dlmopen()
	 * a few times, then switch to a dlopen()/dlclose() alternation.
	 *
	 * I'd rather use __atomic_add_fetch() here, but it's not supported in
	 * GCC 4.4.
	 */

	while (gettimeofday(&now, NULL),
	    now.tv_sec < start.tv_sec + looptime) {
		int n = rand() % 6;

		if (lmids[n] == 0)
			lmids[n] = LM_ID_NEWLM;

		if (non_dlm != NULL) {
			dlerror();
			dlclose(non_dlm);
			if ((error = dlerror()) != NULL)
				fprintf(stderr, "Error closing library: %s\n",
				    error);

			__sync_add_and_fetch(&unload_count, 1);
		}

		dlerror();
		if (many_lmids && loaded[n] == NULL) {
			loaded[n] = dlmopen(lmids[n], "build/libproc-dlmlib.so.0", RTLD_NOW);
			fprintf(stderr, "opened in lmid %i: %p\n", n, loaded[n]);
		}
		else
			non_dlm = dlopen("build/libproc-dlmlib.so.0", RTLD_NOW);
		if ((error = dlerror()) != NULL)
			fprintf(stderr, "Error opening library: %s\n",
			    error);
		__sync_add_and_fetch(&load_count, 1);
	}
	return NULL;
}

int main (int argc, char *argv[])
{
	int num_threads = (getenv("NUM_THREADS") != NULL) ?
	    strtol(getenv("NUM_THREADS"), NULL, 10) : 1;
	pthread_t children[num_threads];

	many_lmids = (getenv("MANY_LMIDS") != NULL);
	if (argc == 2)
		looptime = strtol(argv[1], NULL, 10);

	srand(time(NULL));

	if (num_threads > 1) {
		size_t i;
		for (i = 0; i < num_threads; i++)
			if (pthread_create(&children[i], NULL, churn,
				    NULL) < 0) {
				perror("creating thread");
				exit(1);
			}
		for (i = 0; i < num_threads; i++)
			pthread_join(children[i], NULL);
	} else
		churn(NULL);

	printf("%i loads, %i unloads\n", load_count, unload_count);

	if (argc == 1) {
		char buf[32];

		fprintf(stderr, "Execing...\n");
		snprintf(buf, sizeof(buf), "%li", looptime);
		execl(argv[0], argv[0], buf, NULL);
	}

	return 0;
}
