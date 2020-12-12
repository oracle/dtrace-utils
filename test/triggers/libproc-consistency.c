/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <setjmp.h>

#include <libproc.h>
#include <rtld_db.h>

/*
 * The default single-threaded unwinder pad is fine for us, and we are linked
 * directly to libproc so can fish it straight out without registering a new
 * unwinder pad function and all that rigmarole.
 */
extern libproc_unwinder_pad_fun *libproc_unwinder_pad;

static int nonzero;
static int lmids_fell;
static int many_lmids;

static int
print_ldd(const rd_loadobj_t *loadobj, size_t num, void *p)
{
	static long highest;
	struct ps_prochandle *P = p;
	char buf[PATH_MAX];

	/*
	 * Only print info for libraries until we start to see LMIDs lower than
	 * we already have, to avoid flooding the log.  (If multiple lmids are
	 * not in use, use the iteration counter instead: lmids_fell is a
	 * misnomer in this case.)
	 *
	 * (We still traverse all others and read their strings, thus testing
	 * that the link map is in fact uncorrupted.)
	 */

	if (Pread_string(P, buf, sizeof(buf), loadobj->rl_nameaddr) < 0) {
		fprintf(stderr, "Failed to read string at %lx\n",
		    loadobj->rl_nameaddr);
		return 0;
	}

	if ((many_lmids && highest > loadobj->rl_lmident) ||
	    (!many_lmids && highest > num))
		lmids_fell = 1;
	else if (many_lmids)
		highest = loadobj->rl_lmident;
	else
		highest = num;

	if (loadobj->rl_lmident != 0)
		nonzero++;

	if (!lmids_fell)
		printf("%s: dyn 0x%lx, bias 0x%lx, LMID %li\n", buf, loadobj->rl_dyn,
		    loadobj->rl_diff_addr, loadobj->rl_lmident);
	return 1;
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
	struct ps_prochandle * volatile P_preserved = NULL;
	struct rd_agent *rd;
	jmp_buf exec_jmp;
	int err;
	int implausibly_low;
	jmp_buf ** volatile jmp_pad;

	many_lmids = (getenv("MANY_LMIDS") != NULL);

	if (argc < 2) {
		fprintf(stderr, "Syntax: libproc-consistency process [args ...]\n");
		exit(1);
	}

	P = Pcreate(argv[1], &argv[1], NULL, &err);

	if (!P) {
		fprintf(stderr, "Cannot execute: %s\n", strerror(err));
		exit(1);
	}

	rd = rd_new(P);
	if (!rd) {
		fprintf(stderr, "Initialization failed.\n");
		return 1;
	}
	rd_event_enable(rd, rtld_load_unload, NULL);
 	Ptrace_set_detached(P, 1);
	Puntrace(P, 0);
	jmp_pad = (jmp_buf ** volatile)libproc_unwinder_pad(P);

	if (setjmp(exec_jmp)) {
		pid_t pid;
		/*
		 * The jmp_pad is not reset during the throw: we must reset it
		 * now, in case the rd_new() or other operation within the
		 * exec-spotted path is interrupted by another exec().
		 */
		*jmp_pad = (jmp_buf * volatile)&exec_jmp;
		P = (struct ps_prochandle *)P_preserved;
		pid = Pgetpid(P);

		fprintf(stderr, "Spotted exec()\n");
		/*
		 * We spotted an exec(). Kill the ps_prochandle and make a new
		 * one.
		 */
		Prelease(P, PS_RELEASE_NO_DETACH);
		Pfree(P);
		P = Pgrab(pid, 0, 1, NULL, &err);
		if (!P) {
			perror("Cannot regrab after exec()");
			return 1;
		}
		rd = rd_new(P);
		if (!rd) {
			fprintf(stderr, "Initialization failed.\n");
			return 1;
		}
		rd_event_enable(rd, rtld_load_unload, NULL);
		Ptrace_set_detached(P, 1);
		Puntrace(P, 0);
	}
	*jmp_pad = (jmp_buf * volatile)&exec_jmp;
	P_preserved = P;

	/*
	 * Continuously try to iterate over the link map, doing Pwait()s to run
	 * breakpoint handlers.  Bugs relating to a failure to latch a
	 * consistent state tend to be signalled by a hang inside
	 * rd_ldso_consistent_begin(), which is exposed as rd_loadobj_iter()
	 * taking far too long.  Ignore simple rd_loadobj_iter() errors -- these
	 * are probably just a sign of lag synchronizing a dying tracee with
	 * this testcase: dtrace will have no problem with that.
	 */
	while (Pstate(P) != PS_DEAD) {
		struct timeval a, b;
		gettimeofday(&a, NULL);
		rd_loadobj_iter(rd, print_ldd, P);
		gettimeofday(&b, NULL);
 		if (b.tv_sec - 2 > a.tv_sec) {
			fprintf(stderr, "rd_loadobj_iter took implausibly "
			    "long: %li seconds.\n", (long)(b.tv_sec - a.tv_sec));
			err = 1;
		}
		Pwait(P, 0);
	}

	Prelease(P, PS_RELEASE_KILL);
	Pfree(P);

	printf("%i adds, %i deletes, %i stables, %i nonzero lmids\n", adds_seen,
	    deletes_seen, stables_seen, nonzero);

	/*
	 * Spot an outright failure to latch the dynamic linker state
	 * breakpoint.  Use a much lower definition of 'implausibly low' when
	 * multiple lmids are in use, because the child spends so much time
	 * halted in that case.
	 */

	implausibly_low = (many_lmids ? 5 : 50);

	if (adds_seen < implausibly_low ||
	    deletes_seen < implausibly_low ||
	    stables_seen < implausibly_low) {
		fprintf(stderr, "Implausibly few adds/deletes/stables seen\n");
		err = 1;
	}

	/*
	 * Spot a failure to traverse nonzero lmids -- or, if many_lmids is off,
	 * spot any nonzero lmids at all.
	 */
	if (many_lmids && nonzero == 0) {
		fprintf(stderr, "No nonzero lmids seen!");
		err = 1;
	} else if (!many_lmids && nonzero != 0) {
		fprintf(stderr, "Nonzero lmids seen!");
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
