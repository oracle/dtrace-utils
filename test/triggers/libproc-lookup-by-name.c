/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
	while (Pstate(P) == PS_RUN)
		Pwait(P, 1, NULL);

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

	kill(Pgetpid(P), SIGCONT);
	do
		Pwait(P, 1, NULL);
	while (Pstate(P) == PS_RUN);

	Prelease(P, PS_RELEASE_KILL);
	Pfree(P);

	return 0;
}
