/*
 * Oracle Linux DTrace.
 * Copyright © 2013, 2014, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <libproc.h>
#include <rtld_db.h>

static int libs_seen;

struct rd_and_p {
	struct ps_prochandle *P;
	rd_agent_t *rd;
};

static int
print_ldd(const rd_loadobj_t *loadobj, size_t num, void *state)
{
	struct rd_and_p *rp = state;
	size_t i;
	char buf[PATH_MAX];
	rd_loadobj_t scope = {0};

	if (Pread_string(rp->P, buf, sizeof (buf), loadobj->rl_nameaddr) < 0) {
		fprintf(stderr, "Failed to read string at %lx\n",
		    loadobj->rl_nameaddr);
		return (0);
	}

	/*
	 * No name, no search path: vDSO, we don't care about it.
	 */
	if (buf[0] == '\0' && loadobj->rl_nscopes == 1)
		return (1);

	printf("%s: dyn 0x%lx, bias 0x%lx, LMID %li: %s (", buf, loadobj->rl_dyn,
	    loadobj->rl_diff_addr, loadobj->rl_lmident, loadobj->rl_default_scope ?
	    "inherited symbol search path: ": "symbol search path: ");

	for (i = 0; i < loadobj->rl_nscopes; i++) {
		if (rd_get_scope(rp->rd, &scope, loadobj, i) == NULL)
			fprintf(stderr, "Scope %li -> NULL\n", i);
		else {
			if (Pread_string(rp->P, buf, sizeof (buf), scope.rl_nameaddr) < 0) {
				fprintf(stderr, "Failed to read string at %lx\n",
				    scope.rl_nameaddr);
				return (0);
			}
			printf("%s%s", i > 0 ? ", ": "", buf);
		}
	}
	free(scope.rl_scope);
	printf(")\n");
	libs_seen++;
	return (1);
}

static int
note_ldd(const rd_loadobj_t *loadobj, size_t num, void *state)
{
	struct rd_and_p *rp = state;
	char buf[PATH_MAX];

	if (Pread_string(rp->P, buf, sizeof (buf), loadobj->rl_nameaddr) < 0) {
		fprintf(stderr, "Failed to read string at %lx\n",
		    loadobj->rl_nameaddr);
		return (0);
	}

	/*
	 * No name, no search path: vDSO, we don't care about it.
	 */
	if (buf[0] == '\0' && loadobj->rl_nscopes == 1)
		return (1);

	libs_seen++;
	return (1);
}

static int
do_nothing(const rd_loadobj_t *loadobj, size_t num, void *state)
{
    return (1);
}

int
main(int argc, char *argv[])
{
	long pid;
	struct ps_prochandle *P;
	struct rd_agent *rd;
	int err;
	int really_seen;
	struct rd_and_p rp;

	if (argc < 2) {
		fprintf(stderr, "Syntax: libproc-pldd PID | process [args ...]\n");
		exit(1);
	}

	pid = strtol(argv[1], NULL, 10);

	if (!pid)
		P = Pcreate(argv[1], &argv[1], NULL, &err);
	else
		P = Pgrab(pid, 0, 0, NULL, &err);

	if (!P) {
		fprintf(stderr, "Cannot execute: %s\n", strerror(err));
		exit(1);
	}

	rd = rd_new(P);
	if (!rd) {
		fprintf(stderr, "Initialization failed.\n");
		return (1);
	}

	rp.P = P;
	rp.rd = rd;
	/*
	 * Iterate immediately after initialization to enure that, whether we
	 * get RD_OK or RD_NOMAPS, we do not get a crash.  (Note: this may
	 * resume, briefly, in order to ensure that we get a consistent state.)
	 */
	switch (rd_loadobj_iter(rd, do_nothing, &rp)) {
	case RD_OK:
	case RD_NOMAPS:
		break;
	case RD_ERR:
		fprintf(stderr, "Unknown error.\n");
		break;
	}

	Ptrace_set_detached(P, 1);
	Puntrace(P, 0);

	/*
	 * Now iterate and print.
	 */

	while (rd_loadobj_iter(rd, print_ldd, &rp) == RD_NOMAPS)
		sleep (1);

	really_seen = libs_seen;

	/*
	 * Kill rtld_db (disconnecting from the ptrace()d process),
	 * then recreate it and iterate, ensuring that librtld_db can reattach
	 * on its own if it needs to.
	 */

	rd_delete(rd);
	rd = rd_new(P);
	if (!rd) {
		fprintf(stderr, "rd reinitialization failed.\n");
		return (1);
	}

	while (rd_loadobj_iter(rd, note_ldd, &rp) == RD_NOMAPS)
		sleep (1);

	printf("%i libs seen.\n", really_seen);

	if (libs_seen != really_seen * 2)
	    fprintf(stderr, "Post-reattachment initialization saw %i libs, "
		"where first scan saw %i\n", libs_seen - really_seen,
		really_seen);

	Prelease(P, (pid == 0) ? PS_RELEASE_KILL : PS_RELEASE_NORMAL);
	Pfree(P);

	return (0);
}
