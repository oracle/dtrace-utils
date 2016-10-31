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

#include <sys/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <setjmp.h>

#include <libproc.h>

static int hits;

static int
hit_it(uintptr_t addr, void *data)
{
	hits++;
	return PS_RUN;
}

/*
 * Unwinder pad for libproc setjmp() chains.
 */
static __thread jmp_buf *unwinder_pad;

static jmp_buf **
dt_unwinder_pad(struct ps_prochandle *unused)
{
	return &unwinder_pad;
}

int
main(int argc, char *argv[])
{
	struct ps_prochandle * volatile P_preserved = NULL;
	struct ps_prochandle *P;
	const char *symbol;
	GElf_Sym sym;
	prsyminfo_t sip;
	int err;
	volatile int execs = 0;
	jmp_buf exec_jmp;

	if (argc < 3) {
		fprintf(stderr, "Syntax: libproc-execing-bkpts symbol process count "
		    "[args ...]\n");
		exit(1);
	}

	symbol = argv[1];

	Pset_libproc_unwinder_pad(dt_unwinder_pad);
	P = Pcreate(argv[2], &argv[2], NULL, &err);

	if (!P) {
		fprintf(stderr, "Cannot execute %s: %s\n", argv[2], strerror(err));
		exit(1);
	}

	Puntrace(P, 0);

	/*
	 * Constantly add and remove a breakpoint on the specified symbol, as
	 * long as the process is running.  If an exec() is detected, note as
	 * such, but keep going.
	 */
	unwinder_pad = &exec_jmp;
	if (setjmp(exec_jmp)) {
		int err;
		pid_t pid;

		/*
		 * The jmp_pad is not reset during the throw: we must reset it
		 * now, in case the Pgrab() or other operation within the
		 * exec-spotted path is interrupted by another exec().
		 */

		unwinder_pad = &exec_jmp;
		P = (struct ps_prochandle *) P_preserved;
		execs++;
		pid = Pgetpid(P);
		Prelease(P, PS_RELEASE_NO_DETACH);
		Pfree(P);
		if ((P = Pgrab(pid, 0, 1, NULL, &err)) == NULL) {
			fprintf(stderr, "Cannot regrab %s: %s\n", argv[2], strerror(err));
			exit(1);
		}
		Ptrace_set_detached(P, 0);
	}
	P_preserved = P;

	while (Pstate(P) != PS_DEAD) {
		Pwait(P, 0);

		/*
		 * Look up the name.
		 */
		if ((Pxlookup_by_name(P, PR_LMID_EVERY, PR_OBJ_EVERY, symbol, &sym, &sip) != 0) &&
		    (Pstate(P) != PS_DEAD)) {
			fprintf(stderr, "Lookup error.\n");
			exit(1);
		}

		if (Pstate(P) == PS_DEAD)
			break;

		/*
		 * Drop a breakpoint on it.
		 */
		if ((Pbkpt(P, sym.st_value, 0, hit_it, NULL, NULL) != 0) &&
		    (Pstate(P) != PS_DEAD)) {
			fprintf(stderr, "Cannot drop breakpoint.\n");
			exit(1);
		}
		Puntrace(P, 0); /* untracing, if exec()ed last time */
	}

	if ((execs < 500) || (hits < 500))
		fprintf(stderr, "Implausibly few execs() / breakpoint "
		    "hits seen (%i/%i)\n", execs, hits);

	printf("%i exec()s, %i breakpoint hits seen\n", execs, hits);

	Prelease(P, PS_RELEASE_KILL);
	Pfree(P);

	return (0);
}
