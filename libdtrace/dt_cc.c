/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * DTrace D Language Compiler
 *
 * The code in this source file implements the main engine for the D language
 * compiler.  The driver routine for the compiler is dt_compile(), below.  The
 * compiler operates on either stdio FILEs or in-memory strings as its input
 * and can produce either dtrace_prog_t structures from a D program or a single
 * dtrace_difo_t structure from a D expression.  Multiple entry points are
 * provided as wrappers around dt_compile() for the various input/output pairs.
 * The compiler itself is implemented across the following source files:
 *
 * dt_lex.l - lex scanner
 * dt_grammar.y - yacc grammar
 * dt_parser.c - parse tree creation and semantic checking
 * dt_decl.c - declaration stack processing
 * dt_xlator.c - D translator lookup and creation
 * dt_ident.c - identifier and symbol table routines
 * dt_pragma.c - #pragma processing and D pragmas
 * dt_printf.c - D printf() and printa() argument checking and processing
 * dt_cc.c - compiler driver and dtrace_prog_t construction
 * dt_cg.c - DIF code generator
 * dt_as.c - DIF assembler
 * dt_dof.c - dtrace_prog_t -> DOF conversion
 *
 * Several other source files provide collections of utility routines used by
 * these major files.  The compiler itself is implemented in multiple passes:
 *
 * (1) The input program is scanned and parsed by dt_lex.l and dt_grammar.y
 *     and parse tree nodes are constructed using the routines in dt_parser.c.
 *     This node construction pass is described further in dt_parser.c.
 *
 * (2) The parse tree is "cooked" by assigning each clause a context (see the
 *     routine dt_setcontext(), below) based on its probe description and then
 *     recursively descending the tree performing semantic checking.  The cook
 *     routines are also implemented in dt_parser.c and described there.
 *
 * (3) For actions that are DIF expression statements, the DIF code generator
 *     and assembler are invoked to create a finished DIFO for the statement.
 *
 * (4) The dtrace_prog_t data structures for the program clauses and actions
 *     are built, containing pointers to any DIFOs created in step (3).
 *
 * (5) The caller invokes a routine in dt_dof.c to convert the finished program
 *     into DOF format for use in anonymous tracing or enabling in the kernel.
 *
 * In the implementation, steps 2-4 are intertwined in that they are performed
 * in order for each clause as part of a loop that executes over the clauses.
 *
 * The D compiler currently implements nearly no optimization.  The compiler
 * implements integer constant folding as part of pass (1), and a set of very
 * simple peephole optimizations as part of pass (3).  As with any C compiler,
 * a large number of optimizations are possible on both the intermediate data
 * structures and the generated DIF code.  These possibilities should be
 * investigated in the context of whether they will have any substantive effect
 * on the overall DTrace probe effect before they are undertaken.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <ucontext.h>
#include <limits.h>
#include <ctype.h>
#include <dirent.h>
#include <port.h>
#include <dt_pcap.h>
#include <dt_program.h>
#include <dt_provider.h>
#include <dt_probe.h>
#include <dt_printf.h>
#include <dt_pid.h>
#include <dt_grammar.h>
#include <dt_ident.h>
#include <dt_string.h>
#include <dt_impl.h>
#include <dt_dis.h>
#include <dt_cg.h>
#include <dt_dctx.h>
#include <dt_bpf.h>
#include <bpf_asm.h>

extern int yylineno;

/*ARGSUSED*/
static int
dt_idreset(dt_idhash_t *dhp, dt_ident_t *idp, void *ignored)
{
	idp->di_flags &= ~(DT_IDFLG_REF | DT_IDFLG_MOD |
	    DT_IDFLG_DIFR | DT_IDFLG_DIFW);
	return 0;
}

/*ARGSUSED*/
static int
dt_idpragma(dt_idhash_t *dhp, dt_ident_t *idp, void *ignored)
{
	yylineno = idp->di_lineno;
	xyerror(D_PRAGMA_UNUSED, "unused #pragma %s\n", (char *)idp->di_iarg);
	return 0;
}

static dtrace_stmtdesc_t *
dt_stmt_create(dtrace_hdl_t *dtp, dtrace_ecbdesc_t *edp,
    dtrace_attribute_t descattr, dtrace_attribute_t stmtattr)
{
	dtrace_stmtdesc_t *sdp = dtrace_stmt_create(dtp, edp);

	if (sdp == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	assert(yypcb->pcb_stmt == NULL);
	yypcb->pcb_stmt = sdp;
	yypcb->pcb_maxrecs = 0;

	sdp->dtsd_descattr = descattr;
	sdp->dtsd_stmtattr = stmtattr;

	return sdp;
}

static dt_ident_t *
dt_clause_create(dtrace_hdl_t *dtp, dtrace_difo_t *dp)
{
	char		*name;
	int		len;
	dt_ident_t	*idp;

	/*
	 * Finalize the probe data description for the clause.
	 */
	dt_datadesc_finalize(yypcb->pcb_hdl, yypcb->pcb_ddesc);
	dp->dtdo_ddesc = yypcb->pcb_ddesc;
	yypcb->pcb_ddesc = NULL;

	/*
	 * Special case: ERROR probe default clause.
	 */
	if (yypcb->pcb_cflags & DTRACE_C_EPROBE)
		dp->dtdo_ddesc->dtdd_uarg = DT_ECB_ERROR;

	/*
	 * Generate a symbol name.
	 */
	len = snprintf(NULL, 0, "dt_clause_%d", dtp->dt_clause_nextid) + 1;
	name = dt_alloc(dtp, len);
	if (name == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	snprintf(name, len, "dt_clause_%d", dtp->dt_clause_nextid++);

	/*
	 * Add the symbol to the BPF identifier table and associate the DIFO
	 * with the symbol.
	 */
	idp = dt_dlib_add_func(dtp, name);
	dt_free(dtp, name);
	if (idp == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	dt_ident_set_data(idp, dp);

	return idp;
}

static void
dt_compile_one_clause(dtrace_hdl_t *dtp, dt_node_t *cnp, dt_node_t *pnp)
{
	dtrace_ecbdesc_t	*edp;
	dtrace_datadesc_t	*ddp;
	dtrace_stmtdesc_t	*sdp;

	yylineno = pnp->dn_line;
	dt_setcontext(dtp, pnp->dn_desc);
	dt_node_cook(cnp, DT_IDFLG_REF);

	if (DT_TREEDUMP_PASS(dtp, 2)) {
		fprintf(stderr, "Parse tree (Pass 2):\n");
		dt_node_printr(cnp, stderr, 1);
	}

	edp = dt_ecbdesc_create(dtp, pnp->dn_desc);
	if (edp == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	assert(yypcb->pcb_ecbdesc == NULL);
	yypcb->pcb_ecbdesc = edp;

	ddp = dt_datadesc_create(dtp);
	if (ddp == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	assert(yypcb->pcb_ddesc == NULL);
	yypcb->pcb_ddesc = ddp;

	assert(yypcb->pcb_stmt == NULL);
	sdp = dt_stmt_create(dtp, edp, cnp->dn_ctxattr, cnp->dn_attr);

	/*
	 * Compile the clause (predicate and action).
	 */
	dt_cg(yypcb, cnp);
	sdp->dtsd_clause = dt_clause_create(dtp, dt_as(yypcb));

	assert(yypcb->pcb_stmt == sdp);
	if (dtrace_stmt_add(yypcb->pcb_hdl, yypcb->pcb_prog, sdp) != 0)
		longjmp(yypcb->pcb_jmpbuf, dtrace_errno(yypcb->pcb_hdl));

	if (yypcb->pcb_stmt == sdp)
		yypcb->pcb_stmt = NULL;

	assert(yypcb->pcb_ecbdesc == edp);
	dt_ecbdesc_release(dtp, edp);
	dt_endcontext(dtp);
	yypcb->pcb_ecbdesc = NULL;
}

static void
dt_compile_clause(dtrace_hdl_t *dtp, dt_node_t *cnp)
{
	dt_node_t *pnp;

	/* Force the clause to return type 'int'. */
	dt_node_type_assign(cnp, dtp->dt_ints[0].did_ctfp,
				 dtp->dt_ints[0].did_type);

	for (pnp = cnp->dn_pdescs; pnp != NULL; pnp = pnp->dn_list)
		dt_compile_one_clause(dtp, cnp, pnp);
}

static void
dt_compile_xlator(dt_node_t *dnp)
{
	dt_xlator_t *dxp = dnp->dn_xlator;
	dt_node_t *mnp;

	for (mnp = dnp->dn_members; mnp != NULL; mnp = mnp->dn_list) {
		assert(dxp->dx_membdif[mnp->dn_membid] == NULL);
		dt_cg(yypcb, mnp);
		dxp->dx_membdif[mnp->dn_membid] = dt_as(yypcb);
	}
}

void
dt_setcontext(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp)
{
	const dtrace_pattr_t *pap;
	dt_probe_t *prp;
	dt_provider_t *pvp;
	dt_ident_t *idp;
	char attrstr[8];
	int err;

	/*
	 * Both kernel and pid based providers are allowed to have names
	 * ending with what could be interpreted as a number. We assume it's
	 * a pid and that we may need to dynamically create probes for
	 * that process if:
	 *
	 * (1) The provider doesn't exist, or,
	 * (2) The provider exists and has DT_PROVIDER_PID flag set.
	 *
	 * On an error, dt_pid_create_probes() will set the error message
	 * and tag -- we just have to longjmp() out of here.
	 */
	if (pdp->prv && pdp->prv[0] &&
	    isdigit(pdp->prv[strlen(pdp->prv) - 1]) &&
	    ((pvp = dt_provider_lookup(dtp, pdp->prv)) == NULL ||
	     pvp->pv_flags & DT_PROVIDER_PID) &&
	    dt_pid_create_probes((dtrace_probedesc_t *)pdp, dtp, yypcb) != 0) {
		longjmp(yypcb->pcb_jmpbuf, EDT_COMPILER);
	}

	/*
	 * Call dt_probe_info() to get the probe arguments and attributes.  If
	 * a representative probe is found, set 'pap' to the probe provider's
	 * attributes.  Otherwise set 'pap' to default Unstable attributes.
	 */
	if ((prp = dt_probe_info(dtp, pdp, &yypcb->pcb_pinfo)) == NULL) {
		pap = &_dtrace_prvdesc;
		err = dtrace_errno(dtp);
		memset(&yypcb->pcb_pinfo, 0, sizeof(dtrace_probeinfo_t));
		yypcb->pcb_pinfo.dtp_attr = pap->dtpa_provider;
		yypcb->pcb_pinfo.dtp_arga = pap->dtpa_args;
	} else {
		pap = &prp->prov->desc.dtvd_attr;
		err = 0;
	}

	if (err == EDT_NOPROBE && !(yypcb->pcb_cflags & DTRACE_C_ZDEFS)) {
		xyerror(D_PDESC_ZERO, "probe description %s:%s:%s:%s does not "
			"match any probes\n", pdp->prv, pdp->mod, pdp->fun,
			pdp->prb);
	}

	if (err == EDT_BPF)
		longjmp(yypcb->pcb_jmpbuf, err);

	if (err != EDT_NOPROBE && err != EDT_UNSTABLE && err != 0)
		xyerror(D_PDESC_INVAL, "%s\n", dtrace_errmsg(dtp, err));

	dt_dprintf("set context to %s:%s:%s:%s [%u] prp=%p attr=%s argc=%d\n",
		   pdp->prv, pdp->mod, pdp->fun, pdp->prb, pdp->id,
		   (void *)prp, dt_attr_str(yypcb->pcb_pinfo.dtp_attr,
		   attrstr, sizeof(attrstr)), yypcb->pcb_pinfo.dtp_argc);

	/*
	 * Reset the stability attributes of D global variables that vary
	 * based on the attributes of the provider and context itself.
	 */
	if ((idp = dt_idhash_lookup(dtp->dt_globals, "probeprov")) != NULL)
		idp->di_attr = pap->dtpa_provider;
	if ((idp = dt_idhash_lookup(dtp->dt_globals, "probemod")) != NULL)
		idp->di_attr = pap->dtpa_mod;
	if ((idp = dt_idhash_lookup(dtp->dt_globals, "probefunc")) != NULL)
		idp->di_attr = pap->dtpa_func;
	if ((idp = dt_idhash_lookup(dtp->dt_globals, "probename")) != NULL)
		idp->di_attr = pap->dtpa_name;
	if ((idp = dt_idhash_lookup(dtp->dt_globals, "args")) != NULL)
		idp->di_attr = pap->dtpa_args;

	yypcb->pcb_pdesc = pdp;
	yypcb->pcb_probe = prp;
}

/*
 * Reset context-dependent variables and state at the end of cooking a D probe
 * definition clause.  This ensures that external declarations between clauses
 * do not reference any stale context-dependent data from the previous clause.
 */
void
dt_endcontext(dtrace_hdl_t *dtp)
{
	static const char *const cvars[] = {
		"probeprov", "probemod", "probefunc", "probename", "args", NULL
	};

	dt_ident_t *idp;
	int i;

	for (i = 0; cvars[i] != NULL; i++) {
		if ((idp = dt_idhash_lookup(dtp->dt_globals, cvars[i])) != NULL)
			idp->di_attr = _dtrace_defattr;
	}

	yypcb->pcb_pdesc = NULL;
	yypcb->pcb_probe = NULL;
}

static int
dt_reduceid(dt_idhash_t *dhp, dt_ident_t *idp, dtrace_hdl_t *dtp)
{
	if (idp->di_vers != 0 && idp->di_vers > dtp->dt_vmax)
		dt_idhash_delete(dhp, idp);

	return 0;
}

/*
 * When dtrace_setopt() is called for "version", it calls dt_reduce() to remove
 * any identifiers or translators that have been previously defined as bound to
 * a version greater than the specified version.  Therefore, in our current
 * version implementation, establishing a binding is a one-way transformation.
 * In addition, no versioning is currently provided for types as our .d library
 * files do not define any types and we reserve prefixes DTRACE_ and dtrace_
 * for our exclusive use.  If required, type versioning will require more work.
 */
int
dt_reduce(dtrace_hdl_t *dtp, dt_version_t v)
{
	char s[DT_VERSION_STRMAX];
	dt_xlator_t *dxp, *nxp;

	if (v > dtp->dt_vmax)
		return dt_set_errno(dtp, EDT_VERSREDUCED);
	else if (v == dtp->dt_vmax)
		return 0; /* no reduction necessary */

	dt_dprintf("reducing api version to %s\n",
	    dt_version_num2str(v, s, sizeof(s)));

	dtp->dt_vmax = v;

	for (dxp = dt_list_next(&dtp->dt_xlators); dxp != NULL; dxp = nxp) {
		nxp = dt_list_next(dxp);
		if ((dxp->dx_souid.di_vers != 0 && dxp->dx_souid.di_vers > v) ||
		    (dxp->dx_ptrid.di_vers != 0 && dxp->dx_ptrid.di_vers > v))
			dt_list_delete(&dtp->dt_xlators, dxp);
	}

	dt_idhash_iter(dtp->dt_macros, (dt_idhash_f *)dt_reduceid, dtp);
	dt_idhash_iter(dtp->dt_aggs, (dt_idhash_f *)dt_reduceid, dtp);
	dt_idhash_iter(dtp->dt_globals, (dt_idhash_f *)dt_reduceid, dtp);
	dt_idhash_iter(dtp->dt_tls, (dt_idhash_f *)dt_reduceid, dtp);

	return 0;
}

/*
 * Fork and exec the cpp(1) preprocessor to run over the specified input file,
 * and return a FILE handle for the cpp output.  We use the /dev/fd filesystem
 * here to simplify the code by leveraging file descriptor inheritance (using
 * an output pipe would be possible, but would require extra complexity to
 * avoid deadlocking).
 *
 * There is extra complexity here because cpp checks to see if its input file is
 * a regular file, and complains if the number of bytes it reads does not equal
 * the size of said file.  So we must stick a splice(2) before it, to de-
 * regularize the file into a pipe! (A cat(1) would do just as well or a
 * read/write loop, but a splice is more efficient.)
 */
static FILE *
dt_preproc(dtrace_hdl_t *dtp, FILE *ifp)
{
	int argc = dtp->dt_cpp_argc;
	char **argv = alloca(sizeof(char *) * (argc + 5));
	FILE *ofp = tmpfile();
	int pipe_needed = 0;
	int catpipe[2];

	char opath[20]; /* big enough for /dev/fd/ + INT_MAX + \0 */
	char verdef[32]; /* big enough for -D__SUNW_D_VERSION=0x%08x + \0 */

	struct sigaction act, oact;
	sigset_t mask, omask;

	int wstat, estat, junk;
	pid_t catpid = 0;	/* suppress spurious compiler warning */
	pid_t pid;
	off_t off;

	if (argv == NULL || ofp == NULL) {
		dt_set_errno(dtp, errno);
		goto err;
	}

	/*
	 * If the input is a seekable file, see if it is an interpreter file.
	 * If we see #!, seek past the first line because cpp will choke on it.
	 * We start cpp just prior to the \n at the end of this line so that
	 * it still sees the newline, ensuring that #line values are correct.
	 */
	if (isatty(fileno(ifp)) == 0 && (off = ftello(ifp)) != -1) {
		int c;

		pipe_needed = 1;
		if ((c = fgetc(ifp)) == '#' && (c = fgetc(ifp)) == '!') {
			for (off += 2; c != '\n'; off++)
				if ((c = fgetc(ifp)) == EOF)
					break;
			if (c == '\n')
				off--; /* start cpp just prior to \n */
		}
		fflush(ifp);
		/*
		 * Do not touch ifp via stdio from this point on, to avoid
		 * caching or prereading throwing off the file position.
		 */
		lseek(fileno(ifp), off, SEEK_SET);

		if (pipe(catpipe) < 0) {
			dt_set_errno(dtp, errno);
			goto err;
		}
	}
	snprintf(opath, sizeof(opath), "/dev/fd/%d", fileno(ofp));

	memcpy(argv, dtp->dt_cpp_argv, sizeof(char *) * argc);

	snprintf(verdef, sizeof(verdef),
	    "-D__SUNW_D_VERSION=0x%08x", dtp->dt_vmax);
	argv[argc++] = verdef;

	switch (dtp->dt_stdcmode) {
	case DT_STDC_XA:
		argv[argc++] = "-std=c99";
		break;
	case DT_STDC_XS:
		argv[argc++] = "-traditional-cpp";
		break;
	}

	argv[argc++] = "/dev/stdin";
	argv[argc++] = opath;
	argv[argc] = NULL;

	/*
	 * libdtrace must be able to be embedded in other programs that may
	 * include application-specific signal handlers.  Therefore, if we
	 * need to fork to run cpp(1), we must avoid generating a SIGCHLD
	 * that could confuse the containing application.  To do this,
	 * we block SIGCHLD and reset its disposition to SIG_DFL.
	 * We restore our signal state once we are done.
	 */
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	pthread_sigmask(SIG_BLOCK, &mask, &omask);

	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_DFL;
	sigaction(SIGCHLD, &act, &oact);

	if (pipe_needed) {
		if ((catpid = fork()) == -1) {
			sigaction(SIGCHLD, &oact, NULL);
			pthread_sigmask(SIG_SETMASK, &omask, NULL);
			dt_set_errno(dtp, EDT_CPPFORK);
			goto err;
		}

		if (catpid == 0) {
			int ret = 1;

			while (ret > 0) {
				ret = splice(fileno(ifp), NULL, catpipe[1],
				    NULL, PIPE_BUF, SPLICE_F_MORE);
				if (ret < 0) {
					dt_dprintf("error splicing in "
					    "preprocessor catpipe: %s\n",
					    strerror(errno));
					    _exit(1);
				}
			}
			_exit(0);
		}
		close(catpipe[1]);
	}

	if ((pid = fork()) == -1) {
		sigaction(SIGCHLD, &oact, NULL);
		pthread_sigmask(SIG_SETMASK, &omask, NULL);
		dt_set_errno(dtp, EDT_CPPFORK);
		goto err;
	}

	if (pid == 0) {
		if (!pipe_needed) {
			dup2(fileno(ifp), 0);
		} else {
			dup2(catpipe[0], 0);
			close(catpipe[0]);
		}
		close(fileno(ifp));
		execvp(dtp->dt_cpp_path, argv);
		_exit(errno == ENOENT ? 127 : 126);
	}

	do {
		dt_dprintf("waiting for %s (PID %d)\n", dtp->dt_cpp_path,
		    (int)pid);
	} while (waitpid(pid, &wstat, 0) == -1 && errno == EINTR);

	if (pipe_needed) {
		close(catpipe[0]);

		do {
			dt_dprintf("waiting for catpipe (PID %d)\n", (int)catpid);
		} while (waitpid(catpid, &junk, 0) == -1 && errno == EINTR);
	}

	sigaction(SIGCHLD, &oact, NULL);
	pthread_sigmask(SIG_SETMASK, &omask, NULL);

	dt_dprintf("%s returned exit status %i\n", dtp->dt_cpp_path, wstat);
	estat = WIFEXITED(wstat) ? WEXITSTATUS(wstat) : -1;

	if (estat != 0) {
		switch (estat) {
		case 126:
			dt_set_errno(dtp, EDT_CPPEXEC);
			break;
		case 127:
			dt_set_errno(dtp, EDT_CPPENT);
			break;
		default:
			dt_set_errno(dtp, EDT_CPPERR);
		}
		goto err;
	}

	fflush(ofp);
	fseek(ofp, 0, SEEK_SET);
	return ofp;

err:
	if (pipe_needed) {
		close(catpipe[0]);
		close(catpipe[1]);
	}
	fclose(ofp);
	return NULL;
}

void *
dt_compile(dtrace_hdl_t *dtp, int context, dtrace_probespec_t pspec, void *arg,
    uint_t cflags, int argc, char *const argv[], FILE *fp, const char *s)
{
	dt_node_t *dnp;
	dt_decl_t *ddp;
	dt_pcb_t pcb;
	struct yy_buffer_state *strbuf;
	void *rv = NULL;
	int err;

	if ((fp == NULL && s == NULL) || (cflags & ~DTRACE_C_MASK) != 0) {
		dt_set_errno(dtp, EINVAL);
		return NULL;
	}

	if (dt_list_next(&dtp->dt_lib_path) != NULL && dt_load_libs(dtp) != 0)
		return NULL; /* errno is set for us */

	ctf_discard(dtp->dt_cdefs->dm_ctfp);
	ctf_discard(dtp->dt_ddefs->dm_ctfp);

	dt_idhash_iter(dtp->dt_globals, dt_idreset, NULL);
	dt_idhash_iter(dtp->dt_tls, dt_idreset, NULL);

	if (fp && (cflags & DTRACE_C_CPP) && (fp = dt_preproc(dtp, fp)) == NULL)
		return NULL; /* errno is set for us */

	dt_pcb_push(dtp, &pcb);

	pcb.pcb_fileptr = fp;
	pcb.pcb_string = s;
	pcb.pcb_sargc = argc;
	pcb.pcb_sargv = argv;
	pcb.pcb_sflagv = argc ? calloc(argc, sizeof(ushort_t)) : NULL;
	pcb.pcb_pspec = pspec;
	pcb.pcb_cflags = dtp->dt_cflags | cflags;
	pcb.pcb_amin = dtp->dt_amin;
	pcb.pcb_yystate = -1;
	pcb.pcb_context = context;
	pcb.pcb_token = context;

	if (context != DT_CTX_DPROG)
		yybegin(YYS_EXPR);
	else if (cflags & DTRACE_C_CTL)
		yybegin(YYS_CONTROL);
	else
		yybegin(YYS_CLAUSE);

	if ((err = setjmp(yypcb->pcb_jmpbuf)) != 0)
		goto out;

	if (yypcb->pcb_sargc != 0 && yypcb->pcb_sflagv == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	yypcb->pcb_idents = dt_idhash_create("ambiguous", NULL, 0, 0);
	yypcb->pcb_locals = dt_idhash_create("clause local", NULL, 0, UINT_MAX);

	if (yypcb->pcb_idents == NULL || yypcb->pcb_locals == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	/*
	 * Invoke the parser to evaluate the D source code.  If any errors
	 * occur during parsing, an error function will be called and we
	 * will longjmp back to pcb_jmpbuf to abort.  If parsing succeeds,
	 * we optionally display the parse tree if debugging is enabled.
	 */
	if (yypcb->pcb_string)
		strbuf = yy_scan_string(yypcb->pcb_string);
	if (yyparse() != 0 || yypcb->pcb_root == NULL) {
		if (yypcb->pcb_string)
			yy_delete_buffer(strbuf);

		xyerror(D_EMPTY, "empty D program translation unit\n");
	}

	yybegin(YYS_DONE);

	if (yypcb->pcb_string)
		yy_delete_buffer(strbuf);

	if (cflags & DTRACE_C_CTL)
		goto out;

	if (context != DT_CTX_DTYPE && DT_TREEDUMP_PASS(dtp, 1)) {
		fprintf(stderr, "Parse tree (Pass 1):\n");
		dt_node_printr(yypcb->pcb_root, stderr, 1);
	}

	if (yypcb->pcb_pragmas != NULL)
		dt_idhash_iter(yypcb->pcb_pragmas, dt_idpragma, NULL);

	if (argc > 1 && !(yypcb->pcb_cflags & DTRACE_C_ARGREF) &&
	    !(yypcb->pcb_sflagv[argc - 1] & DT_IDFLG_REF)) {
		xyerror(D_MACRO_UNUSED, "extraneous argument '%s' ($%d is "
		    "not referenced)\n", yypcb->pcb_sargv[argc - 1], argc - 1);
	}

	/*
	 * If we have successfully created a parse tree for a D program, loop
	 * over the clauses and actions and instantiate the corresponding
	 * libdtrace program.  If we are parsing a D expression, then we
	 * simply run the code generator and assembler on the resulting tree.
	 */
	switch (context) {
	case DT_CTX_DPROG:
		assert(yypcb->pcb_root->dn_kind == DT_NODE_PROG);

		if ((dnp = yypcb->pcb_root->dn_list) == NULL &&
		    !(yypcb->pcb_cflags & DTRACE_C_EMPTY))
			xyerror(D_EMPTY, "empty D program translation unit\n");

		if ((yypcb->pcb_prog = dt_program_create(dtp)) == NULL)
			longjmp(yypcb->pcb_jmpbuf, dtrace_errno(dtp));

		for (; dnp != NULL; dnp = dnp->dn_list) {
			switch (dnp->dn_kind) {
			case DT_NODE_CLAUSE:
				dt_compile_clause(dtp, dnp);
				break;
			case DT_NODE_XLATOR:
				if (dtp->dt_xlatemode == DT_XL_DYNAMIC)
					dt_compile_xlator(dnp);
				break;
			case DT_NODE_PROVIDER:
				dt_node_cook(dnp, DT_IDFLG_REF);
				break;
			}
		}

		yypcb->pcb_prog->dp_xrefs = yypcb->pcb_asxrefs;
		yypcb->pcb_prog->dp_xrefslen = yypcb->pcb_asxreflen;
		yypcb->pcb_asxrefs = NULL;
		yypcb->pcb_asxreflen = 0;

		rv = yypcb->pcb_prog;
		break;

	case DT_CTX_DEXPR:
		dt_node_cook(yypcb->pcb_root, DT_IDFLG_REF);
		dt_cg(yypcb, yypcb->pcb_root);
		rv = dt_as(yypcb);
		break;

	case DT_CTX_DTYPE:
		ddp = (dt_decl_t *)yypcb->pcb_root; /* root is really a decl */
		err = dt_decl_type(ddp, arg);
		dt_decl_free(ddp);

		if (err != 0)
			longjmp(yypcb->pcb_jmpbuf, EDT_COMPILER);

		rv = NULL;
		break;
	}

out:
	if (context != DT_CTX_DTYPE && DT_TREEDUMP_PASS(dtp, 3) &&
	    yypcb->pcb_root) {
		fprintf(stderr, "Parse tree (Pass 3):\n");
		dt_node_printr(yypcb->pcb_root, stderr, 1);
	}

	if (dtp->dt_cdefs_fd != -1 && (ftruncate(dtp->dt_cdefs_fd, 0) == -1 ||
	    lseek(dtp->dt_cdefs_fd, 0, SEEK_SET) == -1 ||
	    ctf_write(dtp->dt_cdefs->dm_ctfp, dtp->dt_cdefs_fd) == CTF_ERR))
		dt_dprintf("failed to update CTF cache: %s\n", strerror(errno));

	if (dtp->dt_ddefs_fd != -1 && (ftruncate(dtp->dt_ddefs_fd, 0) == -1 ||
	    lseek(dtp->dt_ddefs_fd, 0, SEEK_SET) == -1 ||
	    ctf_write(dtp->dt_ddefs->dm_ctfp, dtp->dt_ddefs_fd) == CTF_ERR))
		dt_dprintf("failed to update CTF cache: %s\n", strerror(errno));

	if (yypcb->pcb_fileptr && (cflags & DTRACE_C_CPP))
		fclose(yypcb->pcb_fileptr); /* close dt_preproc() file */

	dt_pcb_pop(dtp, err);
	dt_set_errno(dtp, err);
	return err ? NULL : rv;
}

dtrace_difo_t *
dt_construct(dtrace_hdl_t *dtp, dt_probe_t *prp, uint_t cflags, dt_ident_t *idp)
{
	dt_pcb_t	pcb;
	dtrace_difo_t	*dp = NULL;
	int		err;

	if ((cflags & ~DTRACE_C_MASK) != 0) {
		dt_set_errno(dtp, EINVAL);
		return NULL;
	}

	if (dt_list_next(&dtp->dt_lib_path) != NULL && dt_load_libs(dtp) != 0)
		return NULL;

	ctf_discard(dtp->dt_cdefs->dm_ctfp);
	ctf_discard(dtp->dt_ddefs->dm_ctfp);

	dt_idhash_iter(dtp->dt_globals, dt_idreset, NULL);
	dt_idhash_iter(dtp->dt_tls, dt_idreset, NULL);

	dt_pcb_push(dtp, &pcb);

	pcb.pcb_fileptr = NULL;
	pcb.pcb_string = NULL;
	pcb.pcb_sargc = 0;
	pcb.pcb_sargv = NULL;
	pcb.pcb_sflagv = NULL;
	pcb.pcb_pspec = DTRACE_PROBESPEC_NAME;
	pcb.pcb_cflags = dtp->dt_cflags | cflags;
	pcb.pcb_amin = dtp->dt_amin;
	pcb.pcb_yystate = -1;
	pcb.pcb_context = DT_CTX_DPROG;
	pcb.pcb_token = DT_CTX_DPROG;
	pcb.pcb_probe = prp;
	pcb.pcb_pdesc = prp->desc;

	yybegin(YYS_DONE);

	if ((err = setjmp(yypcb->pcb_jmpbuf)) != 0)
		goto out;

	yypcb->pcb_idents = dt_idhash_create("ambiguous", NULL, 0, 0);
	yypcb->pcb_locals = dt_idhash_create("clause local", NULL, 0, UINT_MAX);

	if (yypcb->pcb_idents == NULL || yypcb->pcb_locals == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	if (cflags & DTRACE_C_CTL)
		goto out;

	if (yypcb->pcb_pragmas != NULL)
		dt_idhash_iter(yypcb->pcb_pragmas, dt_idpragma, NULL);

	dt_setcontext(dtp, yypcb->pcb_pdesc);
	dt_node_root(dt_node_trampoline(prp));
	dt_node_type_assign(yypcb->pcb_root, dtp->dt_ints[0].did_ctfp,
					     dtp->dt_ints[0].did_type);
	dt_cg(yypcb, yypcb->pcb_root);
	dp = dt_as(yypcb);

	/*
	 * If we were called with an identifier, assign the DIFO to it.  We
	 * also must ensure that the identifier is of the correct kind (and
	 * has the proper configuration) - we do this by morphing it into
	 * itself.
	 */
	if (idp != NULL) {
		dt_ident_morph(idp, idp->di_kind, &dt_idops_difo, dtp);
		dt_ident_set_data(idp, dp);
	}

out:
	if (dtp->dt_cdefs_fd != -1 &&
	    (ftruncate(dtp->dt_cdefs_fd, 0) == -1 ||
	     lseek(dtp->dt_cdefs_fd, 0, SEEK_SET) == -1 ||
	     ctf_write(dtp->dt_cdefs->dm_ctfp, dtp->dt_cdefs_fd) == CTF_ERR))
		dt_dprintf("failed to update CTF cache: %s\n", strerror(errno));

	if (dtp->dt_ddefs_fd != -1 &&
	    (ftruncate(dtp->dt_ddefs_fd, 0) == -1 ||
	     lseek(dtp->dt_ddefs_fd, 0, SEEK_SET) == -1 ||
	     ctf_write(dtp->dt_ddefs->dm_ctfp, dtp->dt_ddefs_fd) == CTF_ERR))
		dt_dprintf("failed to update CTF cache: %s\n", strerror(errno));

	dt_endcontext(dtp);

	dt_pcb_pop(dtp, err);
	dt_set_errno(dtp, err);

	return err ? NULL : dp;
}

static int
dt_link_layout(dtrace_hdl_t *dtp, const dtrace_difo_t *dp, uint_t *pcp,
	       uint_t *rcp, uint_t *vcp, dt_ident_t *idp)
{
	uint_t			pc = *pcp;
	uint_t			len = dp->dtdo_brelen;
	const dof_relodesc_t	*rp = dp->dtdo_breltab;
	int			no_deps = 0;

	if (idp != NULL) {
		idp->di_flags |= DT_IDFLG_REF;
		if (idp->di_flags & DT_IDFLG_CGREG)
			no_deps = 1;
	}

	(*pcp) += dp->dtdo_len;
	(*rcp) += len;
	(*vcp) += dp->dtdo_varlen;

	if (no_deps)
		return pc;

	for (; len != 0; len--, rp++) {
		const char	*name = dt_difo_getstr(dp, rp->dofr_name);
		dtrace_difo_t	*rdp;
		int		ipc;

		idp = dt_dlib_get_func(dtp, name);
		if (idp == NULL ||			/* not found */
		    idp->di_kind != DT_IDENT_SYMBOL ||	/* not external sym */
		    idp->di_flags & DT_IDFLG_REF)       /* already seen */
			continue;

		rdp = dt_dlib_get_func_difo(dtp, idp);
		if (rdp == NULL)
			return -1;
		ipc = dt_link_layout(dtp, rdp, pcp, rcp, vcp, idp);
		if (ipc == -1)
			return -1;
		idp->di_id = ipc;
	}

	return pc;
}

static uint64_t boottime = 0;
static int get_boottime() {
	struct timespec t_real, t_boot;

	if (clock_gettime(CLOCK_REALTIME, &t_real))
		return -1;
	if (clock_gettime(CLOCK_MONOTONIC, &t_boot))
		return -1;
	boottime = t_real.tv_sec - t_boot.tv_sec;
	boottime *= 1000000000;
	boottime += t_real.tv_nsec - t_boot.tv_nsec;
	return 0;
}

static int
dt_link_construct(dtrace_hdl_t *dtp, const dt_probe_t *prp, dtrace_difo_t *dp,
		  dt_ident_t *idp, const dtrace_difo_t *sdp, uint_t *pcp,
		  uint_t *rcp, uint_t *vcp, dtrace_epid_t epid, uint_t clid)
{
	uint_t			pc = *pcp;
	uint_t			rc = *rcp;
	uint_t			vc = *vcp;
	uint_t			vlen = sdp->dtdo_varlen;
	dtrace_difv_t		*vp = sdp->dtdo_vartab;
	dtrace_difv_t		*nvp = &dp->dtdo_vartab[vc];
	uint_t			len = sdp->dtdo_brelen;
	const dof_relodesc_t	*rp = sdp->dtdo_breltab;
	dof_relodesc_t		*nrp = &dp->dtdo_breltab[rc];
	dtrace_id_t		prid = prp->desc->id;
	int			no_deps = 0;

	if (idp != NULL) {
		idp->di_flags |= DT_IDFLG_REF;
		if (idp->di_flags & DT_IDFLG_CGREG)
			no_deps = 1;
	}

	/*
	 * Make sure there is enough room in the destination instruction buffer
	 * and then copy the executable code for the current function into the
	 * final instruction buffer at the given program counter (instruction
	 * index).
	 */
	assert(pc + sdp->dtdo_len <= dp->dtdo_len);
	(*pcp) += sdp->dtdo_len;
	memcpy(dp->dtdo_buf + pc, sdp->dtdo_buf,
	       sdp->dtdo_len * sizeof(struct bpf_insn));

	/*
	 * Populate the (new) string table with any variable names for this
	 * DIFO.
	 */
	(*vcp) += vlen;
	for (; vlen != 0; vlen--, vp++, nvp++) {
		const char	*name = dt_difo_getstr(sdp, vp->dtdv_name);

		*nvp = *vp;
		nvp->dtdv_name = dt_strtab_insert(dtp->dt_ccstab, name);
		nvp->dtdv_insn_from += pc;
		nvp->dtdv_insn_to += pc;
	}

	/*
	 * Next we copy the relocation table.  We do that before we run through
	 * it to process dependencies because we want relocations to be in
	 * strict ascending order by offset in the final relocation table.  The
	 * disassembler depends on that, as does the relocation resolver
	 * implementation.
	 */
	(*rcp) += len;
	for (; len != 0; len--, rp++, nrp++) {
		const char	*name = dt_difo_getstr(sdp, rp->dofr_name);
		dt_ident_t	*idp = dt_dlib_get_func(dtp, name);

		nrp->dofr_name = dt_strtab_insert(dtp->dt_ccstab, name);
		nrp->dofr_type = rp->dofr_type;
		nrp->dofr_offset = rp->dofr_offset +
				   pc * sizeof(struct bpf_insn);
		nrp->dofr_data = rp->dofr_data;

		if (idp == NULL ||			/* not found */
		    idp->di_kind != DT_IDENT_SYMBOL)	/* not external sym */
			continue;

		nrp->dofr_data = idp->di_id;		/* set value */
	}

	/*
	 * Now go through the relocations once more, performing the actual
	 * work of adding executable code (and relocations) for dependencies.
	 * We also set the destructive flag on the DIFO if any of the
	 * dependencies have the flag set.
	 */
	len = sdp->dtdo_brelen;
	rp = sdp->dtdo_breltab;
	nrp = &dp->dtdo_breltab[rc];
	for (; len != 0; len--, rp++, nrp++) {
		const char	*name = dt_difo_getstr(sdp, rp->dofr_name);
		dtrace_difo_t	*rdp;
		dtrace_epid_t	nepid;
		int		ipc;

		idp = dt_dlib_get_sym(dtp, name);
		if (idp == NULL)			/* not found */
			continue;

		switch (idp->di_kind) {
		case DT_IDENT_SCALAR:			/* constant */
			if (no_deps) {
				nrp->dofr_type = R_BPF_NONE;
				continue;
			}

			switch (idp->di_id) {
			case DT_CONST_EPID:
				nrp->dofr_data = epid;
				continue;
			case DT_CONST_PRID:
				nrp->dofr_data = prp->desc->id;
				continue;
			case DT_CONST_CLID:
				nrp->dofr_data = clid;
				continue;
			case DT_CONST_ARGC:
				nrp->dofr_data = 0;	/* FIXME */
				continue;
			case DT_CONST_STBSZ:
				nrp->dofr_data = dtp->dt_strlen;
				continue;
			case DT_CONST_STRSZ:
				nrp->dofr_data =
					dtp->dt_options[DTRACEOPT_STRSIZE];
				continue;
			case DT_CONST_TUPSZ:
				nrp->dofr_data = DMEM_TUPLE_SZ(dtp);
				continue;
			case DT_CONST_NSPEC:
				nrp->dofr_data = dtp->dt_options[DTRACEOPT_NSPEC];
				continue;
			case DT_CONST_NCPUS:
				nrp->dofr_data = dtp->dt_conf.max_cpuid + 1;
				continue;
			case DT_CONST_STKSIZ:
				nrp->dofr_data = sizeof(uint64_t)
				    * dtp->dt_options[DTRACEOPT_MAXFRAMES];
				continue;
			case DT_CONST_BOOTTM:
				if (boottime == 0 && get_boottime())
					return -1;
				nrp->dofr_data = boottime;
				continue;
			case DT_CONST_PC:
				nrp->dofr_data = nrp->dofr_offset /
						 sizeof(struct bpf_insn);
				continue;
			case DT_CONST_TASK_PID:
			case DT_CONST_TASK_TGID:
			case DT_CONST_TASK_REAL_PARENT:
			case DT_CONST_TASK_COMM: {
				ctf_file_t *cfp = dtp->dt_shared_ctf;
				ctf_id_t type;
				ctf_membinfo_t ctm;
				int rc = 0;

				if (!cfp)
					return dt_set_errno(dtp, EDT_NOCTF);

				type = ctf_lookup_by_name(cfp, "struct task_struct");
				if (type == CTF_ERR)
					goto err_ctf;

				switch (idp->di_id) {
				case DT_CONST_TASK_PID:
					rc = ctf_member_info(cfp, type, "pid", &ctm);
					break;
				case DT_CONST_TASK_TGID:
					rc = ctf_member_info(cfp, type, "tgid", &ctm);
					break;
				case DT_CONST_TASK_REAL_PARENT:
					rc = ctf_member_info(cfp, type, "real_parent", &ctm);
					break;
				case DT_CONST_TASK_COMM:
					rc = ctf_member_info(cfp, type, "comm", &ctm);
					break;
				}
				if (rc == CTF_ERR)
					goto err_ctf;
				nrp->dofr_data = ctm.ctm_offset / NBBY;
				continue;
			}
			case DT_CONST_MUTEX_OWNER: {
				ctf_file_t *cfp = dtp->dt_shared_ctf;
				ctf_id_t type;
				ctf_membinfo_t ctm;
				int rc;

				if (!cfp)
					return dt_set_errno(dtp, EDT_NOCTF);

				type = ctf_lookup_by_name(cfp, "struct mutex");
				if (type == CTF_ERR)
					goto err_ctf;

				rc = ctf_member_info(cfp, type, "owner", &ctm);
				if (rc == CTF_ERR)
					goto err_ctf;

				nrp->dofr_data = ctm.ctm_offset / NBBY;
				continue;
			}
			case DT_CONST_RWLOCK_CNTS: {
				/*
				 * For !CONFIG_PREEMPT_RT, in type rwlock_t, we
				 * want the offset to member raw_lock, and then
				 * (in type arch_rwlock_t == struct qrwlock) we
				 * add the offset to member cnts.  (It turns
				 * out the total offset is 0.)
				 *
				 * FIXME: what about CONFIG_PREEMPT_RT?
				 */
				ctf_file_t *cfp = dtp->dt_shared_ctf;
				ctf_id_t type;
				ctf_membinfo_t ctm;
				int rc;
				uint64_t total_offset;

				if (!cfp)
					return dt_set_errno(dtp, EDT_NOCTF);

				type = ctf_lookup_by_name(cfp, "rwlock_t");
				if (type == CTF_ERR)
					goto err_ctf;
				rc = ctf_member_info(cfp, type, "raw_lock", &ctm);
				if (rc == CTF_ERR)
					goto err_ctf;
				total_offset = ctm.ctm_offset / NBBY;

				type = ctf_lookup_by_name(cfp, "arch_rwlock_t");
				if (type == CTF_ERR)
					goto err_ctf;
				rc = ctf_member_info(cfp, type, "cnts", &ctm);
				if (rc == CTF_ERR)
					goto err_ctf;
				total_offset += ctm.ctm_offset / NBBY;

				nrp->dofr_data = total_offset;
				continue;
			}
			case DT_CONST_DCTX_RODATA:
				nrp->dofr_data = DCTX_RODATA;
				continue;
			case DT_CONST_RODATA_OFF:
				nrp->dofr_data = dtp->dt_rooffset;
				continue;
			case DT_CONST_RODATA_SIZE:
				nrp->dofr_data = dtp->dt_rosize;
				continue;
			case DT_CONST_ZERO_OFF:
				nrp->dofr_data = dtp->dt_zerooffset;
				continue;
			case DT_CONST_STACK_OFF:
				nrp->dofr_data = DMEM_STACK(dtp);
				continue;
			default:
				/* probe name -> value is probe id */
				if (strchr(idp->di_name, ':') != NULL)
					prid = rp->dofr_data;
				continue;
			}

			continue;
		case DT_IDENT_SYMBOL:			/* BPF function */
			if (no_deps) {
				nrp->dofr_data = rp->dofr_data + pc;
				continue;
			}

			if (idp->di_flags & DT_IDFLG_REF)
				continue;

			rdp = dt_dlib_get_func_difo(dtp, idp);
			if (rdp == NULL)
				return -1;
			if (rdp->dtdo_ddesc != NULL) {
				nepid = dt_epid_add(dtp, rdp->dtdo_ddesc, prid);
				clid++;
			} else
				nepid = 0;
			ipc = dt_link_construct(dtp, prp, dp, idp, rdp, pcp,
						rcp, vcp, nepid, clid);
			if (ipc == -1)
				return -1;

			idp->di_id = ipc;
			nrp->dofr_data = idp->di_id;	/* set value */

			if (rdp->dtdo_flags & DIFOFLG_DESTRUCTIVE)
				dp->dtdo_flags |= DIFOFLG_DESTRUCTIVE;

			continue;
		default:
			continue;
		}
	}

	return pc;

 err_ctf:
	dtp->dt_ctferr = ctf_errno(dtp->dt_shared_ctf);
	return -1;
}

static void
dt_link_resolve(dtrace_hdl_t *dtp, dtrace_difo_t *dp)
{
	struct bpf_insn		*buf = dp->dtdo_buf;
	uint_t			len = dp->dtdo_brelen;
	const dof_relodesc_t	*rp = dp->dtdo_breltab;

	for (; len != 0; len--, rp++) {
		const char	*name = dt_difo_getstr(dp, rp->dofr_name);
		dt_ident_t	*idp = dt_dlib_get_sym(dtp, name);
		uint_t		ioff = rp->dofr_offset /
				       sizeof(struct bpf_insn);
		uint64_t	val;

		if (idp == NULL)			/* not found */
			continue;

		/*
		 * We are only relocating constants (EPID and ARGC) and call
		 * instructions to functions that have been linked in.
		 */
		switch (idp->di_kind) {
		case DT_IDENT_SCALAR:
			val = rp->dofr_data;
			break;
		case DT_IDENT_SYMBOL:
			assert(BPF_IS_CALL(buf[ioff]));

			val = rp->dofr_data - ioff - 1;
			break;
		default:
			continue;
		}

		if (rp->dofr_type == R_BPF_64_64) {
			buf[ioff].imm = val & 0xffffffff;
			buf[ioff + 1].imm = val >> 32;
		} else if (rp->dofr_type == R_BPF_64_32)
			buf[ioff].imm = (uint32_t)val;
	}
}

int
dt_link(dtrace_hdl_t *dtp, const dt_probe_t *prp, dtrace_difo_t *dp,
	dt_ident_t *idp)
{
	uint_t		insc = 0;
	uint_t		relc = 0;
	uint_t		varc = 0;
	dtrace_difo_t	*fdp = NULL;
	int		rc;

	/*
	 * Determine the layout of the final (linked) DIFO, and calculate the
	 * total instruction, relocation record, and variable table counts.
	 */
	rc = dt_link_layout(dtp, dp, &insc, &relc, &varc, idp);
	dt_dlib_reset(dtp, B_TRUE);
	if (rc == -1)
		goto fail;

	/*
	 * Allocate memory for constructing the final DIFO.
	 */
	fdp = dt_zalloc(dtp, sizeof(dtrace_difo_t));
	if (fdp == NULL)
		goto nomem;
	fdp->dtdo_buf = dt_calloc(dtp, insc, sizeof(struct bpf_insn));
	if (fdp->dtdo_buf == NULL)
		goto nomem;
	fdp->dtdo_len = insc;
	if (relc) {
		fdp->dtdo_breltab = dt_calloc(dtp, relc,
					      sizeof(dof_relodesc_t));
		if (fdp->dtdo_breltab == NULL)
			goto nomem;
	}
	fdp->dtdo_brelen = relc;
	if (varc) {
		fdp->dtdo_vartab = dt_calloc(dtp, varc, sizeof(dtrace_difv_t));
		if (fdp->dtdo_vartab == NULL)
			goto nomem;
	}
	fdp->dtdo_varlen = varc;

	/*
	 * Construct the final DIFO (instruction buffer, relocation table,
	 * string table, and variable table).
	 */
	insc = relc = varc = 0;

	rc = dt_link_construct(dtp, prp, fdp, idp, dp, &insc, &relc, &varc, 0,
			       0);
	dt_dlib_reset(dtp, B_FALSE);
	if (rc == -1)
		goto fail;

	/*
	 * Replace the program DIFO instruction buffer, BPF relocation table,
	 * and variable table with the new versions.  Also copy the DIFO flags.
	 */
	dt_free(dtp, dp->dtdo_buf);
	dp->dtdo_buf = fdp->dtdo_buf;
	dp->dtdo_len = fdp->dtdo_len;
	dt_free(dtp, dp->dtdo_vartab);
	dp->dtdo_vartab = fdp->dtdo_vartab;
	dp->dtdo_varlen = fdp->dtdo_varlen;
	dt_free(dtp, dp->dtdo_breltab);
	dp->dtdo_breltab = fdp->dtdo_breltab;
	dp->dtdo_brelen = fdp->dtdo_brelen;
	dp->dtdo_flags = fdp->dtdo_flags;

	/*
	 * Write out the new string table.
	 */
	dt_free(dtp, dp->dtdo_strtab);
	dp->dtdo_strlen = dt_strtab_size(dtp->dt_ccstab);
	if (dp->dtdo_strlen > 0) {
		dp->dtdo_strtab = dt_zalloc(dtp, dp->dtdo_strlen);
		if (dp->dtdo_strtab == NULL)
			goto nomem;
		dt_strtab_write(dtp->dt_ccstab,
				(dt_strtab_write_f *)dt_strtab_copystr,
				dp->dtdo_strtab);
	} else
		dp->dtdo_strtab = NULL;

	/*
	 * Resolve the function relocation records.
	 */
	dt_link_resolve(dtp, dp);

	dt_free(dtp, fdp);

	return 0;

fail:
	if (fdp) {
		dt_free(dtp, fdp->dtdo_breltab);
		dt_free(dtp, fdp->dtdo_vartab);
		dt_free(dtp, fdp->dtdo_buf);
		dt_free(dtp, fdp);
	}

	return rc;

nomem:
	rc = dt_set_errno(dtp, EDT_NOMEM);
	goto fail;
}

dtrace_difo_t *
dt_program_construct(dtrace_hdl_t *dtp, dt_probe_t *prp, uint_t cflags,
		     dt_ident_t *idp)
{
	dtrace_difo_t *dp;

	assert(prp != NULL);

	dp = dt_construct(dtp, prp, cflags, idp);
	if (dp == NULL)
		return NULL;

	DT_DISASM_PROG(dtp, cflags, dp, stderr, idp, prp->desc);

	if (dt_link(dtp, prp, dp, idp) != 0)
		return NULL;

	DT_DISASM_PROG_LINKED(dtp, cflags, dp, stderr, idp, prp->desc);

	return dp;
}

static dtrace_prog_t *
dt_program_compile(dtrace_hdl_t *dtp, dtrace_probespec_t spec, uint_t cflags,
		   int argc, char *const argv[], FILE *fp, const char *s)
{
	dtrace_prog_t	*rv;

	/*
	 * If the 'cpp' option was passed, treat it as equivalent to '-C',
	 * unless a D library is being compiled or the default ERROR probe.
	 * If the 'verbose' option was passed, treat it as equivalent to '-S',
	 * unless a D library is being compiled or the default ERROR probe.
	 */
	if (!(cflags & (DTRACE_C_DLIB | DTRACE_C_EPROBE)))
		cflags |= dtp->dt_cflags & (DTRACE_C_CPP | DTRACE_C_DIFV);

	rv = dt_compile(dtp, DT_CTX_DPROG, spec, NULL, cflags, argc, argv, fp,
			s);
	if (rv == NULL)
		return NULL;

	DT_DISASM_CLAUSE(dtp, cflags, rv, stderr);

	return rv;
}

dtrace_prog_t *
dtrace_program_strcompile(dtrace_hdl_t *dtp, const char *s,
			  dtrace_probespec_t spec, uint_t cflags,
			  int argc, char *const argv[])
{
	return dt_program_compile(dtp, spec, cflags, argc, argv, NULL, s);
}

dtrace_prog_t *
dtrace_program_fcompile(dtrace_hdl_t *dtp, FILE *fp, uint_t cflags,
			int argc, char *const argv[])
{
	return dt_program_compile(dtp, DTRACE_PROBESPEC_NAME, cflags,
				  argc, argv, fp, NULL);
}

int
dtrace_type_strcompile(dtrace_hdl_t *dtp, const char *s, dtrace_typeinfo_t *dtt)
{
	dt_compile(dtp, DT_CTX_DTYPE,
	    DTRACE_PROBESPEC_NONE, dtt, 0, 0, NULL, NULL, s);
	return dtp->dt_errno ? -1 : 0;
}

int
dtrace_type_fcompile(dtrace_hdl_t *dtp, FILE *fp, dtrace_typeinfo_t *dtt)
{
	dt_compile(dtp, DT_CTX_DTYPE,
	    DTRACE_PROBESPEC_NONE, dtt, 0, 0, NULL, fp, NULL);
	return dtp->dt_errno ? -1 : 0;
}
