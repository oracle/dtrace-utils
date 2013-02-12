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

static int
print_ldd(const rd_loadobj_t *loadobj, void *p)
{
	struct ps_prochandle *P = p;
	char buf[PATH_MAX];

	/*
	 * Only print info for those libraries with an LMID >0.
	 *
	 * (We still traverse all others, thus testing that the link map is in
	 * fact uncorrupted.)
	 */
	if (loadobj->rl_lmident == 0)
		return (1);

	if (Pread_string(P, buf, sizeof (buf), loadobj->rl_nameaddr) < 0) {
		fprintf(stderr, "Failed to read string at %lx\n",
		    loadobj->rl_nameaddr);
		return (0);
	}

	printf("%s: dyn 0x%lx, bias 0x%lx, LMID %li\n", buf, loadobj->rl_dyn,
	    loadobj->rl_base, loadobj->rl_lmident);
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
		rd_loadobj_iter(rd, print_ldd, P);
		gettimeofday(&b, NULL);
		if (b.tv_sec - 2 > a.tv_sec) {
			fprintf(stderr, "rd_loadobj_iter took implausibly "
			    "long\n");
			err = 1;
		}
		Pwait(P, 0);
	}

	Prelease(P, 1);

	printf("%i adds, %i deletes, %i stables\n", adds_seen, deletes_seen,
	    stables_seen);

	/*
	 * Spot an outright failure to latch the dynamic linker state
	 * breakpoint.
	 */

	if (adds_seen < 50 || deletes_seen < 50 || stables_seen < 50) {
		fprintf(stderr, "Implausibly few adds/deletes/stables seen\n");
		err = 1;
	}

	return err;
}
