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
#include <elf.h>

#include <libproc.h>

static int hit;

static int
hit_it(uintptr_t addr, void *data)
{
	printf("Address hit.\n");

	hit = 1;
	return PS_STOP;
}

int
main(int argc, char *argv[])
{
	struct ps_prochandle *P;
	Lmid_t lmid;
	const char *symbol;
	GElf_Sym sym;
	prsyminfo_t sip;
	int err;

	if (argc < 3) {
		fprintf(stderr, "Syntax: libproc-lookup-by-name symbol lmid process "
		    "[args ...]\n");
		exit(1);
	}

	symbol = argv[1];
	lmid = strtol(argv[2], NULL, 10);

	P = Pcreate(argv[3], &argv[3], NULL, &err);

	if (!P) {
		fprintf(stderr, "Cannot execute %s: %s\n", argv[2], strerror(err));
		exit(1);
	}

	Puntrace(P, 0);

	/*
	 * Wait until halted and waiting for a SIGCONT.
	 */
	while (Pstate(P) == PS_RUN) {
		Pwait(P, 1);
	}

	/*
	 * Look up the name.
	 */
	if (Pxlookup_by_name(P, lmid, PR_OBJ_EVERY, symbol, &sym, &sip) != 0) {
		fprintf(stderr, "Lookup error.\n");
		exit(1);
	}

	if (lmid != PR_LMID_EVERY && sip.prs_lmid != lmid)
		fprintf(stderr, "lmid error: asked for lmid %li, got back sym "
		    "in lmid %li\n", lmid, sip.prs_lmid);

	/*
	 * Drop a breakpoint on it, and wait until it is hit, or the process
	 * dies.
	 */
	if (Pbkpt(P, sym.st_value, 0, hit_it, NULL, NULL) != 0) {
		fprintf(stderr, "Cannot drop breakpoint.\n");
		exit(1);
	}

	Pbkpt_continue(P);
	do {
		Pwait(P, 1);
	} while (Pstate(P) == PS_RUN);

	Prelease(P, 1);

	return (0);
}
