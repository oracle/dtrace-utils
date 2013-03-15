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
 * Copyright 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <libproc.h>
#include <rtld_db.h>

static int nonzero;
static int lmids_fell;

static int
print_ldd(const rd_loadobj_t *loadobj, size_t num, void *p)
{
	static long highest_lmid;
	struct ps_prochandle *P = p;
	char buf[PATH_MAX];

	/*
	 * Only print info for libraries until we start to see LMIDs lower than
	 * we already have, to avoid flooding the log
	 *
	 * (We still traverse all others and read their strings, thus testing
	 * that the link map is in fact uncorrupted.)
	 */

	if (Pread_string(P, buf, sizeof (buf), loadobj->rl_nameaddr) < 0) {
		fprintf(stderr, "Failed to read string at %lx\n",
		    loadobj->rl_nameaddr);
		return (0);
	}

	if (highest_lmid > loadobj->rl_lmident)
		lmids_fell = 1;
	else
		highest_lmid = loadobj->rl_lmident;

	if (loadobj->rl_lmident != 0)
		nonzero++;

	if (!lmids_fell)
		printf("%s: dyn 0x%lx, bias 0x%lx, LMID %li\n", buf, loadobj->rl_dyn,
		    loadobj->rl_diff_addr, loadobj->rl_lmident);
	return (1);
}

static int adds_seen;
static int deletes_seen;
static int stables_seen;

static void
rtld_load_unload(rd_agent_t *rd, rd_event_msg_t *msg, void *unused)
{
	if (!msg) {
		fprintf(stderr, "Cleanup fired.\n");
		return;
	}

	if (msg->type != RD_DLACTIVITY) {
		fprintf(stderr, "Non-DLACTIVITY rd event seen!\n");
		return;
	}
	switch (msg->state) {
	case RD_CONSISTENT: stables_seen++; break;
	case RD_ADD: adds_seen++; break;
	case RD_DELETE: deletes_seen++; break;
	}
}

int main(int argc, char *argv[])
{
	struct ps_prochandle *P;
	struct rd_agent *rd;
	int err;

	if (argc < 2) {
		fprintf(stderr, "Syntax: libproc-consistency process [args ...]\n");
		exit(1);
	}

	P = Pcreate(argv[1], &argv[1], &err, 0);

	if (!P) {
		fprintf(stderr, "Cannot execute: %s\n", strerror(err));
		exit(1);
	}

	rd = rd_new(P);
	if (!rd) {
		fprintf(stderr, "Initialization failed.\n");
		return (1);
	}
	rd_event_enable(rd, rtld_load_unload, NULL);
 	Ptrace_set_detached(P, 1);
	Puntrace(P, 0);

	/*
	 * Continuously try to iterate over the link map, doing Pwait()s to run
	 * breakpoint handlers.  Bugs relating to a failure to latch a
	 * consistent state tend to be signalled by a hang inside
	 * rd_ldso_consistent_begin(), which is exposed as rd_loadobj_iter()
	 * taking far too long.
	 */
	while (Pstate(P) != PS_DEAD) {
		struct timeval a, b;
		gettimeofday(&a, NULL);
		if (rd_loadobj_iter(rd, print_ldd, P) == RD_ERR)
			fprintf(stderr, "rd_loadobj_iter() returned error\n");
		gettimeofday(&b, NULL);
 		if (b.tv_sec - 2 > a.tv_sec) {
			fprintf(stderr, "rd_loadobj_iter took implausibly "
			    "long\n");
			err = 1;
		}
		Pwait(P, 0);
	}

	Prelease(P, 1);

	printf("%i adds, %i deletes, %i stables, %i nonzero lmids\n", adds_seen,
	    deletes_seen, stables_seen, nonzero);

	/*
	 * Spot an outright failure to latch the dynamic linker state
	 * breakpoint.
	 */

	if (adds_seen < 50 || deletes_seen < 50 || stables_seen < 50) {
		fprintf(stderr, "Implausibly few adds/deletes/stables seen\n");
		err = 1;
	}

	/*
	 * Spot a failure to traverse nonzero lmids.
	 */
	if (nonzero == 0) {
		fprintf(stderr, "No nonzero lmids seen!");
		err = 1;
	}

	/*
	 * Spot a failure to traverse lmids more than once.
	 */
	else if (!lmids_fell) {
		fprintf(stderr, "lmids never seen to fall, despite repeated "
		    "iterations!\n");
		err = 1;
	}

	return err;
}
