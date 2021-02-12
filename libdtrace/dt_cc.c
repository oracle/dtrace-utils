/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2021, Oracle and/or its affiliates. All rights reserved.
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

#ifdef FIXME
/*
 * Utility function to determine if a given action description is destructive.
 * The DIFOFLG_DESTRUCTIVE bit is set for us by the DIF assembler (see dt_as.c).
 */
static int
dt_action_destructive(const dtrace_actdesc_t *ap)
{
	return (DTRACEACT_ISDESTRUCTIVE(ap->dtad_kind) ||
		(ap->dtad_kind == DTRACEACT_DIFEXPR &&
		 (ap->dtad_difo->dtdo_flags & DIFOFLG_DESTRUCTIVE)));
}
#endif

static void
dt_stmt_append(dtrace_stmtdesc_t *sdp, const dt_node_t *dnp)
{
#ifdef FIXME
	dtrace_ecbdesc_t *edp = sdp->dtsd_ecbdesc;
	dtrace_actdesc_t *ap, *tap;
	int commit = 0;
	int speculate = 0;
	int datarec = 0;

	/*
	 * Make sure that the new statement jibes with the rest of the ECB.
	 * FIXME: Eliminate this code once it's incorporated elsewhere.
	 */
	for (ap = edp->dted_action; ap != NULL; ap = ap->dtad_next) {
		if (ap->dtad_kind == DTRACEACT_COMMIT) {
			if (commit) {
				dnerror(dnp, D_COMM_COMM, "commit( ) may "
				    "not follow commit( )\n");
			}

			if (datarec) {
				dnerror(dnp, D_COMM_DREC, "commit( ) may "
				    "not follow data-recording action(s)\n");
			}

			for (tap = ap; tap != NULL; tap = tap->dtad_next) {
				if (!DTRACEACT_ISAGG(tap->dtad_kind))
					continue;

				dnerror(dnp, D_AGG_COMM, "aggregating actions "
				    "may not follow commit( )\n");
			}

			commit = 1;
			continue;
		}

		if (ap->dtad_kind == DTRACEACT_SPECULATE) {
			if (speculate) {
				dnerror(dnp, D_SPEC_SPEC, "speculate( ) may "
				    "not follow speculate( )\n");
			}

			if (commit) {
				dnerror(dnp, D_SPEC_COMM, "speculate( ) may "
				    "not follow commit( )\n");
			}

			if (datarec) {
				dnerror(dnp, D_SPEC_DREC, "speculate( ) may "
				    "not follow data-recording action(s)\n");
			}

			speculate = 1;
			continue;
		}

		if (DTRACEACT_ISAGG(ap->dtad_kind)) {
			if (speculate) {
				dnerror(dnp, D_AGG_SPEC, "aggregating actions "
				    "may not follow speculate( )\n");
			}

			datarec = 1;
			continue;
		}

		if (speculate) {
			if (dt_action_destructive(ap)) {
				dnerror(dnp, D_ACT_SPEC, "destructive actions "
				    "may not follow speculate( )\n");
			}

			if (ap->dtad_kind == DTRACEACT_EXIT) {
				dnerror(dnp, D_EXIT_SPEC, "exit( ) may not "
				    "follow speculate( )\n");
			}
		}

		/*
		 * Exclude all non data-recording actions.
		 */
		if (dt_action_destructive(ap) ||
		    ap->dtad_kind == DTRACEACT_DISCARD)
			continue;

		if (ap->dtad_kind == DTRACEACT_DIFEXPR &&
		    ap->dtad_difo->dtdo_rtype.dtdt_kind == DIF_TYPE_CTF &&
		    ap->dtad_difo->dtdo_rtype.dtdt_size == 0)
			continue;

		if (commit) {
			dnerror(dnp, D_DREC_COMM, "data-recording actions "
			    "may not follow commit( )\n");
		}

		if (!speculate)
			datarec = 1;
	}
#endif

	if (dtrace_stmt_add(yypcb->pcb_hdl, yypcb->pcb_prog, sdp) != 0)
		longjmp(yypcb->pcb_jmpbuf, dtrace_errno(yypcb->pcb_hdl));

	if (yypcb->pcb_stmt == sdp)
		yypcb->pcb_stmt = NULL;
}

#ifdef FIXME
/*
 * For the first element of an aggregation tuple or for printa(), we create a
 * simple DIF program that simply returns the immediate value that is the ID
 * of the aggregation itself.  This could be optimized in the future by
 * creating a new in-kernel dtad_kind that just returns an integer.
 */
static void
dt_action_difconst(dtrace_actdesc_t *ap, uint_t id, dtrace_actkind_t kind)
{
	dtrace_hdl_t *dtp = yypcb->pcb_hdl;
	dtrace_difo_t *dp = dt_zalloc(dtp, sizeof(dtrace_difo_t));

	if (dp == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	dp->dtdo_buf = dt_calloc(dtp, 2, sizeof(dif_instr_t));
	if (dp->dtdo_buf == NULL) {
		dt_difo_free(dtp, dp);
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);
	}

	dp->dtdo_buf[0] = BPF_MOV_IMM(BPF_REG_0, 0);	/* mov %r0, 0 */
	dp->dtdo_buf[1] = BPF_RETURN();			/* exit	*/
	dp->dtdo_len = 2;
	dp->dtdo_rtype = dt_int_rtype;

	ap->dtad_difo = dp;
	ap->dtad_kind = kind;
}

static void
dt_action_clear(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dt_ident_t *aid;
	dtrace_actdesc_t *ap;
	dt_node_t *anp;

	char n[DT_TYPE_NAMELEN];

	anp = dnp->dn_args;
	assert(anp != NULL);

	if (anp->dn_kind != DT_NODE_AGG)
		dnerror(dnp, D_CLEAR_AGGARG,
		    "%s( ) argument #1 is incompatible with prototype:\n"
		    "\tprototype: aggregation\n\t argument: %s\n",
		    dnp->dn_ident->di_name,
		    dt_node_type_name(anp, n, sizeof(n)));

	aid = anp->dn_ident;

	if (aid->di_gen == dtp->dt_gen && !(aid->di_flags & DT_IDFLG_MOD))
		dnerror(dnp, D_CLEAR_AGGBAD,
		    "undefined aggregation: @%s\n", aid->di_name);

	ap = dt_stmt_action(dtp, sdp);
	dt_action_difconst(ap, anp->dn_ident->di_id, DTRACEACT_LIBACT);
	ap->dtad_arg = DT_ACT_CLEAR;
}

static void
dt_action_normalize(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dt_ident_t *aid;
	dtrace_actdesc_t *ap;
	dt_node_t *anp, *normal;
	int denormal = (strcmp(dnp->dn_ident->di_name, "denormalize") == 0);

	char n[DT_TYPE_NAMELEN];
	int argc = 0;

	for (anp = dnp->dn_args; anp != NULL; anp = anp->dn_list)
		argc++; /* count up arguments for error messages below */

	if ((denormal && argc != 1) || (!denormal && argc != 2)) {
		dnerror(dnp, D_NORMALIZE_PROTO,
		    "%s( ) prototype mismatch: %d args passed, %d expected\n",
		    dnp->dn_ident->di_name, argc, denormal ? 1 : 2);
	}

	anp = dnp->dn_args;
	assert(anp != NULL);

	if (anp->dn_kind != DT_NODE_AGG) {
		dnerror(dnp, D_NORMALIZE_AGGARG,
		    "%s( ) argument #1 is incompatible with prototype:\n"
		    "\tprototype: aggregation\n\t argument: %s\n",
		    dnp->dn_ident->di_name,
		    dt_node_type_name(anp, n, sizeof(n)));
	}

	if ((normal = anp->dn_list) != NULL && !dt_node_is_scalar(normal)) {
		dnerror(dnp, D_NORMALIZE_SCALAR,
		    "%s( ) argument #2 must be of scalar type\n",
		    dnp->dn_ident->di_name);
	}

	aid = anp->dn_ident;

	if (aid->di_gen == dtp->dt_gen && !(aid->di_flags & DT_IDFLG_MOD)) {
		dnerror(dnp, D_NORMALIZE_AGGBAD,
		    "undefined aggregation: @%s\n", aid->di_name);
	}

	ap = dt_stmt_action(dtp, sdp);
	dt_action_difconst(ap, anp->dn_ident->di_id, DTRACEACT_LIBACT);

	if (denormal) {
		ap->dtad_arg = DT_ACT_DENORMALIZE;
		return;
	}

	ap->dtad_arg = DT_ACT_NORMALIZE;

	assert(normal != NULL);
	ap = dt_stmt_action(dtp, sdp);
	dt_cg(yypcb, normal);

	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_LIBACT;
	ap->dtad_arg = DT_ACT_NORMALIZE;
}

static void
dt_action_trunc(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dt_ident_t *aid;
	dtrace_actdesc_t *ap;
	dt_node_t *anp, *trunc;

	char n[DT_TYPE_NAMELEN];
	int argc = 0;

	for (anp = dnp->dn_args; anp != NULL; anp = anp->dn_list)
		argc++; /* count up arguments for error messages below */

	if (argc > 2 || argc < 1) {
		dnerror(dnp, D_TRUNC_PROTO,
		    "%s( ) prototype mismatch: %d args passed, %s expected\n",
		    dnp->dn_ident->di_name, argc,
		    argc < 1 ? "at least 1" : "no more than 2");
	}

	anp = dnp->dn_args;
	assert(anp != NULL);
	trunc = anp->dn_list;

	if (anp->dn_kind != DT_NODE_AGG) {
		dnerror(dnp, D_TRUNC_AGGARG,
		    "%s( ) argument #1 is incompatible with prototype:\n"
		    "\tprototype: aggregation\n\t argument: %s\n",
		    dnp->dn_ident->di_name,
		    dt_node_type_name(anp, n, sizeof(n)));
	}

	if (argc == 2) {
		assert(trunc != NULL);
		if (!dt_node_is_scalar(trunc)) {
			dnerror(dnp, D_TRUNC_SCALAR,
			    "%s( ) argument #2 must be of scalar type\n",
			    dnp->dn_ident->di_name);
		}
	}

	aid = anp->dn_ident;

	if (aid->di_gen == dtp->dt_gen && !(aid->di_flags & DT_IDFLG_MOD)) {
		dnerror(dnp, D_TRUNC_AGGBAD,
		    "undefined aggregation: @%s\n", aid->di_name);
	}

	ap = dt_stmt_action(dtp, sdp);
	dt_action_difconst(ap, anp->dn_ident->di_id, DTRACEACT_LIBACT);
	ap->dtad_arg = DT_ACT_TRUNC;

	ap = dt_stmt_action(dtp, sdp);

	if (argc == 1) {
		dt_action_difconst(ap, 0, DTRACEACT_LIBACT);
	} else {
		assert(trunc != NULL);
		dt_cg(yypcb, trunc);
		ap->dtad_difo = dt_as(yypcb);
		ap->dtad_kind = DTRACEACT_LIBACT;
	}

	ap->dtad_arg = DT_ACT_TRUNC;
}

static void
dt_action_printa(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dt_ident_t *aid, *fid;
	dtrace_actdesc_t *ap;
	const char *format;
	dt_node_t *anp, *proto = NULL;

	char n[DT_TYPE_NAMELEN];
	int argc = 0, argr = 0;

	for (anp = dnp->dn_args; anp != NULL; anp = anp->dn_list)
		argc++; /* count up arguments for error messages below */

	switch (dnp->dn_args->dn_kind) {
	case DT_NODE_STRING:
		format = dnp->dn_args->dn_string;
		anp = dnp->dn_args->dn_list;
		argr = 2;
		break;
	case DT_NODE_AGG:
		format = NULL;
		anp = dnp->dn_args;
		argr = 1;
		break;
	default:
		format = NULL;
		anp = dnp->dn_args;
		argr = 1;
	}

	if (argc < argr) {
		dnerror(dnp, D_PRINTA_PROTO,
		    "%s( ) prototype mismatch: %d args passed, %d expected\n",
		    dnp->dn_ident->di_name, argc, argr);
	}

	assert(anp != NULL);

	while (anp != NULL) {
		if (anp->dn_kind != DT_NODE_AGG) {
			dnerror(dnp, D_PRINTA_AGGARG,
			    "%s( ) argument #%d is incompatible with "
			    "prototype:\n\tprototype: aggregation\n"
			    "\t argument: %s\n", dnp->dn_ident->di_name, argr,
			    dt_node_type_name(anp, n, sizeof(n)));
		}

		aid = anp->dn_ident;
		fid = aid->di_iarg;

		if (aid->di_gen == dtp->dt_gen &&
		    !(aid->di_flags & DT_IDFLG_MOD)) {
			dnerror(dnp, D_PRINTA_AGGBAD,
			    "undefined aggregation: @%s\n", aid->di_name);
		}

		/*
		 * If we have multiple aggregations, we must be sure that
		 * their key signatures match.
		 */
		if (proto != NULL) {
			dt_printa_validate(proto, anp);
		} else {
			proto = anp;
		}

		if (format != NULL) {
			yylineno = dnp->dn_line;

			sdp->dtsd_fmtdata =
			    dt_printf_create(yypcb->pcb_hdl, format);
			dt_printf_validate(sdp->dtsd_fmtdata,
			    DT_PRINTF_AGGREGATION, dnp->dn_ident, 1,
			    fid->di_id, ((dt_idsig_t *)aid->di_data)->dis_args);
			format = NULL;
		}

		ap = dt_stmt_action(dtp, sdp);
		dt_action_difconst(ap, anp->dn_ident->di_id, DTRACEACT_PRINTA);

		anp = anp->dn_list;
		argr++;
	}
}

static void
dt_action_printflike(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp,
    dtrace_actkind_t kind)
{
	dt_node_t *anp, *arg1;
	dtrace_actdesc_t *ap = NULL;
	char n[DT_TYPE_NAMELEN], *str;

	assert(DTRACEACT_ISPRINTFLIKE(kind));

	if (dnp->dn_args->dn_kind != DT_NODE_STRING) {
		dnerror(dnp, D_PRINTF_ARG_FMT,
		    "%s( ) argument #1 is incompatible with prototype:\n"
		    "\tprototype: string constant\n\t argument: %s\n",
		    dnp->dn_ident->di_name,
		    dt_node_type_name(dnp->dn_args, n, sizeof(n)));
	}

	arg1 = dnp->dn_args->dn_list;
	yylineno = dnp->dn_line;
	str = dnp->dn_args->dn_string;


	/*
	 * If this is an freopen(), we use an empty string to denote that
	 * stdout should be restored.  For other printf()-like actions, an
	 * empty format string is illegal:  an empty format string would
	 * result in malformed DOF, and the compiler thus flags an empty
	 * format string as a compile-time error.  To avoid propagating the
	 * freopen() special case throughout the system, we simply transpose
	 * an empty string into a sentinel string (DT_FREOPEN_RESTORE) that
	 * denotes that stdout should be restored.
	 */
	if (kind == DTRACEACT_FREOPEN) {
		if (strcmp(str, DT_FREOPEN_RESTORE) == 0) {
			/*
			 * Our sentinel is always an invalid argument to
			 * freopen(), but if it's been manually specified, we
			 * must fail now instead of when the freopen() is
			 * actually evaluated.
			 */
			dnerror(dnp, D_FREOPEN_INVALID,
			    "%s( ) argument #1 cannot be \"%s\"\n",
			    dnp->dn_ident->di_name, DT_FREOPEN_RESTORE);
		}

		if (str[0] == '\0')
			str = DT_FREOPEN_RESTORE;
	}

	sdp->dtsd_fmtdata = dt_printf_create(dtp, str);

	dt_printf_validate(sdp->dtsd_fmtdata, DT_PRINTF_EXACTLEN,
	    dnp->dn_ident, 1, DTRACEACT_AGGREGATION, arg1);

	if (arg1 == NULL) {
		struct bpf_insn *dbuf;
		dtrace_difo_t *dp;

		if ((dbuf = dt_alloc(dtp, sizeof(struct bpf_insn))) == NULL ||
		    (dp = dt_zalloc(dtp, sizeof(dtrace_difo_t))) == NULL) {
			dt_free(dtp, dbuf);
			longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);
		}

		dbuf[0] = BPF_RETURN();		/* ret %r0 */

		dp->dtdo_buf = dbuf;
		dp->dtdo_len = 1;
		dp->dtdo_rtype = dt_int_rtype;

		ap = dt_stmt_action(dtp, sdp);
		ap->dtad_difo = dp;
		ap->dtad_kind = kind;
		return;
	}

	for (anp = arg1; anp != NULL; anp = anp->dn_list) {
		ap = dt_stmt_action(dtp, sdp);
		dt_cg(yypcb, anp);
		ap->dtad_difo = dt_as(yypcb);
		ap->dtad_kind = kind;
	}
}

static void
dt_action_trace(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	if (dt_node_is_void(dnp->dn_args)) {
		dnerror(dnp->dn_args, D_TRACE_VOID,
		    "trace( ) may not be applied to a void expression\n");
	}

	if (dt_node_is_dynamic(dnp->dn_args)) {
		dnerror(dnp->dn_args, D_TRACE_DYN,
		    "trace( ) may not be applied to a dynamic expression\n");
	}

	dt_cg(yypcb, dnp->dn_args);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_DIFEXPR;
}

static void
dt_action_tracemem(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap;

	dt_node_t *addr = dnp->dn_args;
	dt_node_t *size = dnp->dn_args->dn_list;
	dt_node_t *dsize;

	char n[DT_TYPE_NAMELEN];

	if (dt_node_is_integer(addr) == 0 && dt_node_is_pointer(addr) == 0) {
		dnerror(addr, D_TRACEMEM_ADDR,
		    "tracemem( ) argument #1 is incompatible with "
		    "prototype:\n\tprototype: pointer or integer\n"
		    "\t argument: %s\n",
		    dt_node_type_name(addr, n, sizeof(n)));
	}

	if (dt_node_is_posconst(size) == 0) {
		dnerror(size, D_TRACEMEM_SIZE, "tracemem( ) argument #2 must "
		    "be a non-zero positive integral constant expression\n");
	}

	ap = dt_stmt_action(dtp, sdp);
	dt_cg(yypcb, addr);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_TRACEMEM;
	ap->dtad_difo->dtdo_rtype.dtdt_flags |= DIF_TF_BYREF;
	ap->dtad_difo->dtdo_rtype.dtdt_size = size->dn_value;

	if ((dsize = size->dn_list) != NULL) {
		ctf_file_t *fp = dsize->dn_ctfp;
		ctf_id_t type = dsize->dn_type;
		ctf_encoding_t e;

		ap->dtad_arg = DTRACE_TRACEMEM_DYNAMIC;

		ap = dt_stmt_action(dtp, sdp);
		dt_cg(yypcb, dsize);
		ap->dtad_difo = dt_as(yypcb);
		ap->dtad_kind = DTRACEACT_TRACEMEM;

		if (ctf_type_encoding(fp, ctf_type_resolve(fp, type), &e) == CTF_ERR)
			longjmp(yypcb->pcb_jmpbuf, EDT_CTF);

		if (e.cte_format & CTF_INT_SIGNED)
			ap->dtad_arg = DTRACE_TRACEMEM_SSIZE;
		else
			ap->dtad_arg = DTRACE_TRACEMEM_SIZE;
	} else {
		ap->dtad_arg = DTRACE_TRACEMEM_STATIC;
	}
}

static void
dt_action_stack_args(dtrace_hdl_t *dtp, dtrace_actdesc_t *ap, dt_node_t *arg0)
{
	ap->dtad_kind = DTRACEACT_STACK;

	if (dtp->dt_options[DTRACEOPT_STACKFRAMES] != DTRACEOPT_UNSET) {
		ap->dtad_arg = dtp->dt_options[DTRACEOPT_STACKFRAMES];
	} else {
		ap->dtad_arg = 0;
	}

	if (arg0 != NULL) {
		if (arg0->dn_list != NULL) {
			dnerror(arg0, D_STACK_PROTO, "stack( ) prototype "
			    "mismatch: too many arguments\n");
		}

		if (dt_node_is_posconst(arg0) == 0) {
			dnerror(arg0, D_STACK_SIZE, "stack( ) size must be a "
			    "non-zero positive integral constant expression\n");
		}

		ap->dtad_arg = arg0->dn_value;
	}
}

static void
dt_action_stack(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);
	dt_action_stack_args(dtp, ap, dnp->dn_args);
}

static void
dt_action_ustack_args(dtrace_hdl_t *dtp, dtrace_actdesc_t *ap, dt_node_t *dnp)
{
	uint32_t nframes = 0;
	uint32_t strsize = 0;	/* default string table size */
	dt_node_t *arg0 = dnp->dn_args;
	dt_node_t *arg1 = arg0 != NULL ? arg0->dn_list : NULL;

	assert(dnp->dn_ident->di_id == DT_ACT_JSTACK ||
	    dnp->dn_ident->di_id == DT_ACT_USTACK);

	if (dnp->dn_ident->di_id == DT_ACT_JSTACK) {
		if (dtp->dt_options[DTRACEOPT_JSTACKFRAMES] != DTRACEOPT_UNSET)
			nframes = dtp->dt_options[DTRACEOPT_JSTACKFRAMES];

		if (dtp->dt_options[DTRACEOPT_JSTACKSTRSIZE] != DTRACEOPT_UNSET)
			strsize = dtp->dt_options[DTRACEOPT_JSTACKSTRSIZE];

		ap->dtad_kind = DTRACEACT_JSTACK;
	} else {
		assert(dnp->dn_ident->di_id == DT_ACT_USTACK);

		if (dtp->dt_options[DTRACEOPT_USTACKFRAMES] != DTRACEOPT_UNSET)
			nframes = dtp->dt_options[DTRACEOPT_USTACKFRAMES];

		ap->dtad_kind = DTRACEACT_USTACK;
	}

	if (arg0 != NULL) {
		if (!dt_node_is_posconst(arg0)) {
			dnerror(arg0, D_USTACK_FRAMES, "ustack( ) argument #1 "
			    "must be a non-zero positive integer constant\n");
		}
		nframes = (uint32_t)arg0->dn_value;
	}

	if (arg1 != NULL) {
		if (arg1->dn_kind != DT_NODE_INT ||
		    ((arg1->dn_flags & DT_NF_SIGNED) &&
		    (int64_t)arg1->dn_value < 0)) {
			dnerror(arg1, D_USTACK_STRSIZE, "ustack( ) argument #2 "
			    "must be a positive integer constant\n");
		}

		if (arg1->dn_list != NULL) {
			dnerror(arg1, D_USTACK_PROTO, "ustack( ) prototype "
			    "mismatch: too many arguments\n");
		}

		strsize = (uint32_t)arg1->dn_value;
	}

	ap->dtad_arg = DTRACE_USTACK_ARG(nframes, strsize);
}

static void
dt_action_ustack(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);
	dt_action_ustack_args(dtp, ap, dnp);
}

static void
dt_action_setopt(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap;
	dt_node_t *arg0, *arg1;

	/*
	 * The prototype guarantees that we are called with either one or
	 * two arguments, and that any arguments that are present are strings.
	 */
	arg0 = dnp->dn_args;
	arg1 = arg0->dn_list;

	ap = dt_stmt_action(dtp, sdp);
	dt_cg(yypcb, arg0);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_LIBACT;
	ap->dtad_arg = DT_ACT_SETOPT;

	ap = dt_stmt_action(dtp, sdp);

	if (arg1 == NULL) {
		dt_action_difconst(ap, 0, DTRACEACT_LIBACT);
	} else {
		dt_cg(yypcb, arg1);
		ap->dtad_difo = dt_as(yypcb);
		ap->dtad_kind = DTRACEACT_LIBACT;
	}

	ap->dtad_arg = DT_ACT_SETOPT;
}

/*ARGSUSED*/
static void
dt_action_symmod_args(dtrace_hdl_t *dtp, dtrace_actdesc_t *ap,
    dt_node_t *dnp, dtrace_actkind_t kind)
{
	assert(kind == DTRACEACT_SYM || kind == DTRACEACT_MOD ||
	    kind == DTRACEACT_USYM || kind == DTRACEACT_UMOD ||
	    kind == DTRACEACT_UADDR);

	dt_cg(yypcb, dnp);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = kind;
	ap->dtad_difo->dtdo_rtype.dtdt_size = sizeof(uint64_t);
}

static void
dt_action_symmod(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp,
    dtrace_actkind_t kind)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);
	dt_action_symmod_args(dtp, ap, dnp->dn_args, kind);
}

/*ARGSUSED*/
static void
dt_action_ftruncate(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	/*
	 * Library actions need a DIFO that serves as an argument.  As
	 * ftruncate() doesn't take an argument, we generate the constant 0
	 * in a DIFO; this constant will be ignored when the ftruncate() is
	 * processed.
	 */
	dt_action_difconst(ap, 0, DTRACEACT_LIBACT);
	ap->dtad_arg = DT_ACT_FTRUNCATE;
}

/*ARGSUSED*/
static void
dt_action_stop(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	ap->dtad_kind = DTRACEACT_STOP;
	ap->dtad_arg = 0;
}

/*ARGSUSED*/
static void
dt_action_breakpoint(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	ap->dtad_kind = DTRACEACT_BREAKPOINT;
	ap->dtad_arg = 0;
}

/*ARGSUSED*/
static void
dt_action_panic(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	ap->dtad_kind = DTRACEACT_PANIC;
	ap->dtad_arg = 0;
}

static void
dt_action_pcap(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap;
	dt_node_t *addr = dnp->dn_args;
	dt_node_t *anp, *proto;
	char n[DT_TYPE_NAMELEN];
	int argc = 0;

	for (anp = dnp->dn_args; anp != NULL; anp = anp->dn_list)
		argc++;

	if (argc != 2) {
		dnerror(dnp, D_PCAP_PROTO,
			"%s( ) prototype mismatch: %d args passed, 2 expected\n",
			dnp->dn_ident->di_name, argc);
	}

	if (dt_node_is_integer(addr) == 0 && dt_node_is_pointer(addr) == 0) {
		dnerror(addr, D_PCAP_ADDR,
			"pcap( ) argument #1 is incompatible with "
			"prototype:\n\tprototype: pointer or integer\n"
			"\t argument: %s\n",
			dt_node_type_name(addr, n, sizeof(n)));
	}

	proto = addr->dn_list;

	ap = dt_stmt_action(dtp, sdp);
	dt_cg(yypcb, addr);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_PCAP;
	ap->dtad_difo->dtdo_rtype.dtdt_flags |= DIF_TF_BYREF;

	/*
	 * Enough space for 64-bit timestamp, len and pkt data.
	 */

	ap->dtad_difo->dtdo_rtype.dtdt_size = (2 * sizeof(uint64_t)) +
	    DT_PCAPSIZE(dtp->dt_options[DTRACEOPT_PCAPSIZE]);
	ap->dtad_arg = 0;

	ap = dt_stmt_action(dtp, sdp);
	dt_cg(yypcb, proto);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_PCAP;
}

static void
dt_action_chill(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	dt_cg(yypcb, dnp->dn_args);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_CHILL;
}

static void
dt_action_raise(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	dt_cg(yypcb, dnp->dn_args);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_RAISE;
}

static void
dt_action_exit(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	dt_cg(yypcb, dnp->dn_args);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_EXIT;
	ap->dtad_difo->dtdo_rtype.dtdt_size = sizeof(int);
}

static void
dt_action_speculate(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	dt_cg(yypcb, dnp->dn_args);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_SPECULATE;
}

static void
dt_action_commit(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	dt_cg(yypcb, dnp->dn_args);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_COMMIT;
}

static void
dt_action_discard(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	dt_cg(yypcb, dnp->dn_args);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_kind = DTRACEACT_DISCARD;
}

static void
dt_compile_fun(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	switch (dnp->dn_expr->dn_ident->di_id) {
	case DT_ACT_BREAKPOINT:
		dt_action_breakpoint(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_CHILL:
		dt_action_chill(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_CLEAR:
		dt_action_clear(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_COMMIT:
		dt_action_commit(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_DENORMALIZE:
		dt_action_normalize(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_DISCARD:
		dt_action_discard(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_EXIT:
		dt_action_exit(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_FREOPEN:
		dt_action_printflike(dtp, dnp->dn_expr, sdp, DTRACEACT_FREOPEN);
		break;
	case DT_ACT_FTRUNCATE:
		dt_action_ftruncate(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_MOD:
		dt_action_symmod(dtp, dnp->dn_expr, sdp, DTRACEACT_MOD);
		break;
	case DT_ACT_NORMALIZE:
		dt_action_normalize(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_PANIC:
		dt_action_panic(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_PCAP:
		dt_action_pcap(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_PRINTA:
		dt_action_printa(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_PRINTF:
		dt_action_printflike(dtp, dnp->dn_expr, sdp, DTRACEACT_PRINTF);
		break;
	case DT_ACT_RAISE:
		dt_action_raise(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_SETOPT:
		dt_action_setopt(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_SPECULATE:
		dt_action_speculate(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_STACK:
		dt_action_stack(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_STOP:
		dt_action_stop(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_SYM:
		dt_action_symmod(dtp, dnp->dn_expr, sdp, DTRACEACT_SYM);
		break;
	case DT_ACT_SYSTEM:
		dt_action_printflike(dtp, dnp->dn_expr, sdp, DTRACEACT_SYSTEM);
		break;
	case DT_ACT_TRACE:
		dt_action_trace(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_TRACEMEM:
		dt_action_tracemem(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_TRUNC:
		dt_action_trunc(dtp, dnp->dn_expr, sdp);
		break;
	case DT_ACT_UADDR:
		dt_action_symmod(dtp, dnp->dn_expr, sdp, DTRACEACT_UADDR);
		break;
	case DT_ACT_UMOD:
		dt_action_symmod(dtp, dnp->dn_expr, sdp, DTRACEACT_UMOD);
		break;
	case DT_ACT_USYM:
		dt_action_symmod(dtp, dnp->dn_expr, sdp, DTRACEACT_USYM);
		break;
	case DT_ACT_USTACK:
	case DT_ACT_JSTACK:
		dt_action_ustack(dtp, dnp->dn_expr, sdp);
		break;
	default:
		dnerror(dnp->dn_expr, D_UNKNOWN, "tracing function %s( ) is "
		    "not yet supported\n", dnp->dn_expr->dn_ident->di_name);
	}
}
#endif

#ifdef FIXME
static void
dt_compile_exp(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dtrace_actdesc_t *ap = dt_stmt_action(dtp, sdp);

	dt_cg(yypcb, dnp->dn_expr);
	ap->dtad_difo = dt_as(yypcb);
	ap->dtad_difo->dtdo_rtype = dt_void_rtype;
	ap->dtad_kind = DTRACEACT_DIFEXPR;
}

static void
dt_compile_agg(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_stmtdesc_t *sdp)
{
	dt_ident_t *aid, *fid;
	dt_node_t *anp, *incr = NULL;
	dtrace_actdesc_t *ap;
	uint_t n = 1, argmax;
	uint64_t arg = 0;

	/*
	 * If the aggregation has no aggregating function applied to it, then
	 * this statement has no effect.  Flag this as a programming error.
	 */
	if (dnp->dn_aggfun == NULL) {
		dnerror(dnp, D_AGG_NULL, "expression has null effect: @%s\n",
		    dnp->dn_ident->di_name);
	}

	aid = dnp->dn_ident;
	fid = dnp->dn_aggfun->dn_ident;

	if (dnp->dn_aggfun->dn_args != NULL &&
	    dt_node_is_scalar(dnp->dn_aggfun->dn_args) == 0) {
		dnerror(dnp->dn_aggfun, D_AGG_SCALAR, "%s( ) argument #1 must "
		    "be of scalar type\n", fid->di_name);
	}

	/*
	 * The ID of the aggregation itself is implicitly recorded as the first
	 * member of each aggregation tuple so we can distinguish them later.
	 */
	ap = dt_stmt_action(dtp, sdp);
	dt_action_difconst(ap, aid->di_id, DTRACEACT_DIFEXPR);

	for (anp = dnp->dn_aggtup; anp != NULL; anp = anp->dn_list) {
		ap = dt_stmt_action(dtp, sdp);
		n++;

		if (anp->dn_kind == DT_NODE_FUNC) {
			if (anp->dn_ident->di_id == DT_ACT_STACK) {
				dt_action_stack_args(dtp, ap, anp->dn_args);
				continue;
			}

			if (anp->dn_ident->di_id == DT_ACT_USTACK ||
			    anp->dn_ident->di_id == DT_ACT_JSTACK) {
				dt_action_ustack_args(dtp, ap, anp);
				continue;
			}

			switch (anp->dn_ident->di_id) {
			case DT_ACT_UADDR:
				dt_action_symmod_args(dtp, ap,
				    anp->dn_args, DTRACEACT_UADDR);
				continue;

			case DT_ACT_USYM:
				dt_action_symmod_args(dtp, ap,
				    anp->dn_args, DTRACEACT_USYM);
				continue;

			case DT_ACT_UMOD:
				dt_action_symmod_args(dtp, ap,
				    anp->dn_args, DTRACEACT_UMOD);
				continue;

			case DT_ACT_SYM:
				dt_action_symmod_args(dtp, ap,
				    anp->dn_args, DTRACEACT_SYM);
				continue;

			case DT_ACT_MOD:
				dt_action_symmod_args(dtp, ap,
				    anp->dn_args, DTRACEACT_MOD);
				continue;

			default:
				break;
			}
		}

		dt_cg(yypcb, anp);
		ap->dtad_difo = dt_as(yypcb);
		ap->dtad_kind = DTRACEACT_DIFEXPR;
	}

	if (fid->di_id == DTRACEAGG_LQUANTIZE) {
		/*
		 * For linear quantization, we have between two and four
		 * arguments in addition to the expression:
		 *
		 *    arg1 => Base value
		 *    arg2 => Limit value
		 *    arg3 => Quantization level step size (defaults to 1)
		 *    arg4 => Quantization increment value (defaults to 1)
		 */
		dt_node_t *arg1 = dnp->dn_aggfun->dn_args->dn_list;
		dt_node_t *arg2 = arg1->dn_list;
		dt_node_t *arg3 = arg2->dn_list;
		dt_idsig_t *isp;
		uint64_t nlevels, step = 1, oarg;
		int64_t baseval, limitval;

		if (arg1->dn_kind != DT_NODE_INT) {
			dnerror(arg1, D_LQUANT_BASETYPE, "lquantize( ) "
			    "argument #1 must be an integer constant\n");
		}

		baseval = (int64_t)arg1->dn_value;

		if (baseval < INT32_MIN || baseval > INT32_MAX) {
			dnerror(arg1, D_LQUANT_BASEVAL, "lquantize( ) "
			    "argument #1 must be a 32-bit quantity\n");
		}

		if (arg2->dn_kind != DT_NODE_INT) {
			dnerror(arg2, D_LQUANT_LIMTYPE, "lquantize( ) "
			    "argument #2 must be an integer constant\n");
		}

		limitval = (int64_t)arg2->dn_value;

		if (limitval < INT32_MIN || limitval > INT32_MAX) {
			dnerror(arg2, D_LQUANT_LIMVAL, "lquantize( ) "
			    "argument #2 must be a 32-bit quantity\n");
		}

		if (limitval < baseval) {
			dnerror(dnp, D_LQUANT_MISMATCH,
			    "lquantize( ) base (argument #1) must be less "
			    "than limit (argument #2)\n");
		}

		if (arg3 != NULL) {
			if (!dt_node_is_posconst(arg3)) {
				dnerror(arg3, D_LQUANT_STEPTYPE, "lquantize( ) "
				    "argument #3 must be a non-zero positive "
				    "integer constant\n");
			}

			if ((step = arg3->dn_value) > UINT16_MAX) {
				dnerror(arg3, D_LQUANT_STEPVAL, "lquantize( ) "
				    "argument #3 must be a 16-bit quantity\n");
			}
		}

		nlevels = (limitval - baseval) / step;

		if (nlevels == 0) {
			dnerror(dnp, D_LQUANT_STEPLARGE,
			    "lquantize( ) step (argument #3) too large: must "
			    "have at least one quantization level\n");
		}

		if (nlevels > UINT16_MAX) {
			dnerror(dnp, D_LQUANT_STEPSMALL, "lquantize( ) step "
			    "(argument #3) too small: number of quantization "
			    "levels must be a 16-bit quantity\n");
		}

		arg = (step << DTRACE_LQUANTIZE_STEPSHIFT) |
		    (nlevels << DTRACE_LQUANTIZE_LEVELSHIFT) |
		    ((baseval << DTRACE_LQUANTIZE_BASESHIFT) &
		    DTRACE_LQUANTIZE_BASEMASK);

		assert(arg != 0);

		isp = (dt_idsig_t *)aid->di_data;

		if (isp->dis_auxinfo == 0) {
			/*
			 * This is the first time we've seen an lquantize()
			 * for this aggregation; we'll store our argument
			 * as the auxiliary signature information.
			 */
			isp->dis_auxinfo = arg;
		} else if ((oarg = isp->dis_auxinfo) != arg) {
			/*
			 * If we have seen this lquantize() before and the
			 * argument doesn't match the original argument, pick
			 * the original argument apart to concisely report the
			 * mismatch.
			 */
			int obaseval = DTRACE_LQUANTIZE_BASE(oarg);
			int onlevels = DTRACE_LQUANTIZE_LEVELS(oarg);
			int ostep = DTRACE_LQUANTIZE_STEP(oarg);

			if (obaseval != baseval) {
				dnerror(dnp, D_LQUANT_MATCHBASE, "lquantize( ) "
				    "base (argument #1) doesn't match previous "
				    "declaration: expected %d, found %d\n",
				    obaseval, (int)baseval);
			}

			if (onlevels * ostep != nlevels * step) {
				dnerror(dnp, D_LQUANT_MATCHLIM, "lquantize( ) "
				    "limit (argument #2) doesn't match previous"
				    " declaration: expected %d, found %d\n",
				    obaseval + onlevels * ostep,
				    (int)baseval + (int)nlevels * (int)step);
			}

			if (ostep != step) {
				dnerror(dnp, D_LQUANT_MATCHSTEP, "lquantize( ) "
				    "step (argument #3) doesn't match previous "
				    "declaration: expected %d, found %d\n",
				    ostep, (int)step);
			}

			/*
			 * We shouldn't be able to get here -- one of the
			 * parameters must be mismatched if the arguments
			 * didn't match.
			 */
			assert(0);
		}

		incr = arg3 != NULL ? arg3->dn_list : NULL;
		argmax = 5;
	}

	if (fid->di_id == DTRACEAGG_LLQUANTIZE) {
		/*
		 * For log linear quantization, we have four
		 * arguments in addition to the expression:
		 *
		 *    arg1 => Factor
		 *    arg2 => Lower magnitude
		 *    arg3 => Upper magnitude
		 *    arg4 => Steps per magnitude
		 */
		dt_node_t *arg1 = dnp->dn_aggfun->dn_args->dn_list;
		dt_node_t *arg2 = arg1->dn_list;
		dt_node_t *arg3 = arg2->dn_list;
		dt_node_t *arg4 = arg3->dn_list;
		dt_idsig_t *isp;
		uint64_t factor, lmag, hmag, steps, oarg;

		if (arg1->dn_kind != DT_NODE_INT) {
			dnerror(arg1, D_LLQUANT_FACTORTYPE, "llquantize( ) "
			    "argument #1 must be an integer constant\n");
		}

		factor = (uint64_t)arg1->dn_value;

		if (factor > UINT16_MAX || factor < 2) {
			dnerror(arg1, D_LLQUANT_FACTORVAL, "llquantize( ) "
			    "argument #1 must be an unsigned 16-bit "
			    "quantity greater than or equal to 2\n");
		}

		if (arg2->dn_kind != DT_NODE_INT) {
			dnerror(arg2, D_LLQUANT_LMAGTYPE, "llquantize( ) "
			    "argument #2 must be an integer constant\n");
		}

		lmag = (uint64_t)arg2->dn_value;

		if (lmag > UINT16_MAX) {
			dnerror(arg2, D_LLQUANT_LMAGVAL, "llquantize( ) "
			    "argument #2 must be an unsigned 16-bit "
			    "quantity\n");
		}

		if (arg3->dn_kind != DT_NODE_INT) {
			dnerror(arg3, D_LLQUANT_HMAGTYPE, "llquantize( ) "
			    "argument #3 must be an integer constant\n");
		}

		hmag = (uint64_t)arg3->dn_value;

		if (hmag > UINT16_MAX) {
			dnerror(arg3, D_LLQUANT_HMAGVAL, "llquantize( ) "
			    "argument #3 must be an unsigned 16-bit "
			    "quantity\n");
		}

		if (hmag < lmag) {
			dnerror(arg3, D_LLQUANT_HMAGVAL, "llquantize( ) "
			    "argument #3 (high magnitude) must be at least "
			    "argument #2 (low magnitude)\n");
		}

		if (powl(factor, hmag + 1) > (long double)UINT64_MAX) {
			dnerror(arg3, D_LLQUANT_HMAGVAL, "llquantize( ) "
			    "argument #3 (high magnitude) will cause overflow "
			    "of 64 bits\n");
		}

		if (arg4->dn_kind != DT_NODE_INT) {
			dnerror(arg4, D_LLQUANT_STEPTYPE, "llquantize( ) "
			    "argument #4 must be an integer constant\n");
		}

		steps = (uint64_t)arg4->dn_value;

		if (steps > UINT16_MAX) {
			dnerror(arg4, D_LLQUANT_STEPVAL, "llquantize( ) "
			    "argument #4 must be an unsigned 16-bit "
			    "quantity\n");
		}

		if (steps < factor) {
			if (factor % steps != 0) {
				dnerror(arg1, D_LLQUANT_STEPVAL, "llquantize() "
				    "argument #4 (steps) must evenly divide "
				    "argument #1 (factor) when steps<factor\n");
			}
		}
		if (steps > factor) {
			if (steps % factor != 0) {
				dnerror(arg1, D_LLQUANT_STEPVAL, "llquantize() "
				    "argument #4 (steps) must be a multiple of "
				    "argument #1 (factor) when steps>factor\n");
			}
			if (lmag == 0) {
				if ((factor * factor) % steps != 0) {
					dnerror(arg1, D_LLQUANT_STEPVAL, "llquantize() "
					    "argument #4 (steps) must evenly divide the square of "
					    "argument #1 (factor) when steps>factor and "
					    "lmag==0\n");
				}
			} else {
				unsigned long long ii = powl(factor, lmag + 1);
				if (ii % steps != 0) {
					dnerror(arg1, D_LLQUANT_STEPVAL, "llquantize() "
					    "argument #4 (steps) must evenly divide pow("
					    "argument #1 (factor), lmag "
					    "(argument #2) + 1) "
					    "when steps>factor and "
					    "lmag==0\n");
				}
			}
		}

		arg = (steps << DTRACE_LLQUANTIZE_STEPSSHIFT |
		    hmag << DTRACE_LLQUANTIZE_HMAGSHIFT |
		    lmag << DTRACE_LLQUANTIZE_LMAGSHIFT |
		    factor << DTRACE_LLQUANTIZE_FACTORSHIFT);

		assert(arg != 0);

		isp = (dt_idsig_t *)aid->di_data;

		if (isp->dis_auxinfo == 0) {
			/*
			 * This is the first time we've seen an llquantize()
			 * for this aggregation; we'll store our argument
			 * as the auxiliary signature information.
			 */
			isp->dis_auxinfo = arg;
		} else if ((oarg = isp->dis_auxinfo) != arg) {
			/*
			 * If we have seen this llquantize() before and the
			 * argument doesn't match the original argument, pick
			 * the original argument apart to concisely report the
			 * mismatch.
			 */
			int ofactor = DTRACE_LLQUANTIZE_FACTOR(oarg);
			int olmag = DTRACE_LLQUANTIZE_LMAG(oarg);
			int ohmag = DTRACE_LLQUANTIZE_HMAG(oarg);
			int osteps = DTRACE_LLQUANTIZE_STEPS(oarg);

			if (ofactor != factor) {
				dnerror(dnp, D_LLQUANT_MATCHFACTOR,
				    "llquantize() factor (argument #1) doesn't "
				    "match previous declaration: expected %d, "
				    "found %d\n", ofactor, (int)factor);
			}

			if (olmag != lmag) {
				dnerror(dnp, D_LLQUANT_MATCHLMAG,
				    "llquantize() lmag (argument #2) doesn't "
				    "match previous declaration: expected %d, "
				    "found %d\n", olmag, (int)lmag);
			}

			if (ohmag != hmag) {
				dnerror(dnp, D_LLQUANT_MATCHHMAG,
				    "llquantize() hmag (argument #3) doesn't "
				    "match previous declaration: expected %d, "
				    "found %d\n", ohmag, (int)hmag);
			}

			if (osteps != steps) {
				dnerror(dnp, D_LLQUANT_MATCHSTEPS,
				    "llquantize() steps (argument #4) doesn't "
				    "match previous declaration: expected %d, "
				    "found %d\n", osteps, (int)steps);
			}

			/*
			 * We shouldn't be able to get here -- one of the
			 * parameters must be mismatched if the arguments
			 * didn't match.
			 */
			assert(0);
		}

		incr = arg4 != NULL ? arg4->dn_list : NULL;
		argmax = 6;
	}

	if (fid->di_id == DTRACEAGG_QUANTIZE) {
		incr = dnp->dn_aggfun->dn_args->dn_list;
		argmax = 2;
	}

	if (incr != NULL) {
		if (!dt_node_is_scalar(incr)) {
			dnerror(dnp, D_PROTO_ARG, "%s( ) increment value "
			    "(argument #%d) must be of scalar type\n",
			    fid->di_name, argmax);
		}

		if ((anp = incr->dn_list) != NULL) {
			int argc = argmax;

			for (; anp != NULL; anp = anp->dn_list)
				argc++;

			dnerror(incr, D_PROTO_LEN, "%s( ) prototype "
			    "mismatch: %d args passed, at most %d expected",
			    fid->di_name, argc, argmax);
		}

		ap = dt_stmt_action(dtp, sdp);
		n++;

		dt_cg(yypcb, incr);
		ap->dtad_difo = dt_as(yypcb);
		ap->dtad_difo->dtdo_rtype = dt_void_rtype;
		ap->dtad_kind = DTRACEACT_DIFEXPR;
	}

	assert(sdp->dtsd_aggdata == NULL);
	sdp->dtsd_aggdata = aid;

	ap = dt_stmt_action(dtp, sdp);
	assert(fid->di_kind == DT_IDENT_AGGFUNC);
	assert(DTRACEACT_ISAGG(fid->di_id));
	ap->dtad_kind = fid->di_id;
	ap->dtad_ntuple = n;
	ap->dtad_arg = arg;

	if (dnp->dn_aggfun->dn_args != NULL) {
		dt_cg(yypcb, dnp->dn_aggfun->dn_args);
		ap->dtad_difo = dt_as(yypcb);
	}
}
#endif

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
	dt_stmt_append(sdp, cnp);

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
dt_setcontext(dtrace_hdl_t *dtp, dtrace_probedesc_t *pdp)
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
	    dt_pid_create_probes(pdp, dtp, yypcb) != 0) {
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
	pcb.pcb_strptr = s;
	pcb.pcb_strlen = s ? strlen(s) : 0;
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
	yypcb->pcb_locals = dt_idhash_create("clause local", NULL,
					     0, DT_LVAR_MAX);

	if (yypcb->pcb_idents == NULL || yypcb->pcb_locals == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	/*
	 * Invoke the parser to evaluate the D source code.  If any errors
	 * occur during parsing, an error function will be called and we
	 * will longjmp back to pcb_jmpbuf to abort.  If parsing succeeds,
	 * we optionally display the parse tree if debugging is enabled.
	 */
	if (yyparse() != 0 || yypcb->pcb_root == NULL)
		xyerror(D_EMPTY, "empty D program translation unit\n");

	yybegin(YYS_DONE);

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

static dtrace_difo_t *
dt_construct(dtrace_hdl_t *dtp, dt_probe_t *prp, uint_t cflags, dt_ident_t *idp)
{
	dt_pcb_t	pcb;
	dt_node_t	*tnp;
	dtrace_difo_t	*dp;
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
	pcb.pcb_strptr = NULL;
	pcb.pcb_strlen = 0;
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
	pcb.pcb_pdesc = NULL;

	if ((err = setjmp(yypcb->pcb_jmpbuf)) != 0)
		goto out;

	yypcb->pcb_idents = dt_idhash_create("ambiguous", NULL, 0, 0);
	yypcb->pcb_locals = dt_idhash_create("clause local", NULL, 0,
					     DT_LVAR_MAX);

	if (yypcb->pcb_idents == NULL || yypcb->pcb_locals == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	if (cflags & DTRACE_C_CTL)
		goto out;

	if (yypcb->pcb_pragmas != NULL)
		dt_idhash_iter(yypcb->pcb_pragmas, dt_idpragma, NULL);

	tnp = dt_node_trampoline(prp);
	dt_node_type_assign(tnp, dtp->dt_ints[0].did_ctfp,
				 dtp->dt_ints[0].did_type);
	dt_cg(yypcb, tnp);
	dp = dt_as(yypcb);

	/*
	 * If we were called with an identifier, assign the DIFO to it.
	 */
	if (idp != NULL)
		dt_ident_set_data(idp, dp);

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
		char            *name = &dp->dtdo_strtab[rp->dofr_name];
		dtrace_difo_t   *rdp;
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

static int
dt_link_construct(dtrace_hdl_t *dtp, const dt_probe_t *prp, dtrace_difo_t *dp,
		  dt_ident_t *idp, const dtrace_difo_t *sdp, dt_strtab_t *stab,
		  uint_t *pcp, uint_t *rcp, uint_t *vcp, dtrace_epid_t epid,
		  uint_t clid)
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
		const char	*name = &sdp->dtdo_strtab[vp->dtdv_name];

		*nvp = *vp;
		nvp->dtdv_name = dt_strtab_insert(stab, name);
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
		const char	*name = &sdp->dtdo_strtab[rp->dofr_name];
		dt_ident_t	*idp = dt_dlib_get_func(dtp, name);

		nrp->dofr_name = dt_strtab_insert(stab, name);
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
	 */
	len = sdp->dtdo_brelen;
	rp = sdp->dtdo_breltab;
	nrp = &dp->dtdo_breltab[rc];
	for (; len != 0; len--, rp++, nrp++) {
		const char	*name = &sdp->dtdo_strtab[rp->dofr_name];
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
			ipc = dt_link_construct(dtp, prp, dp, idp, rdp, stab,
						pcp, rcp, vcp, nepid, clid);
			if (ipc == -1)
				return -1;

			idp->di_id = ipc;
			nrp->dofr_data = idp->di_id;	/* set value */

			continue;
		default:
			continue;
		}
	}

	return pc;
}

static void
dt_link_resolve(dtrace_hdl_t *dtp, dtrace_difo_t *dp)
{
	struct bpf_insn		*buf = dp->dtdo_buf;
	uint_t			len = dp->dtdo_brelen;
	const dof_relodesc_t	*rp = dp->dtdo_breltab;

	for (; len != 0; len--, rp++) {
		const char	*name = &dp->dtdo_strtab[rp->dofr_name];
		dt_ident_t	*idp = dt_dlib_get_sym(dtp, name);
		uint_t		ioff = rp->dofr_offset /
				       sizeof(struct bpf_insn);
		uint32_t	val;

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
			buf[ioff].imm = val;
			buf[ioff + 1].imm = 0;
		} else if (rp->dofr_type == R_BPF_64_32)
			buf[ioff].imm = val;
	}
}

static int
dt_link(dtrace_hdl_t *dtp, const dt_probe_t *prp, dtrace_difo_t *dp,
	dt_ident_t *idp)
{
	uint_t		insc = 0;
	uint_t		relc = 0;
	uint_t		varc = 0;
	dtrace_difo_t	*fdp = NULL;
	dt_strtab_t	*stab;
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
	stab = dt_strtab_create(BUFSIZ);
	if (stab == NULL)
		goto nomem;

	rc = dt_link_construct(dtp, prp, fdp, idp, dp, stab, &insc, &relc,
			       &varc, 0, 0);
	dt_dlib_reset(dtp, B_FALSE);
	if (rc == -1)
		goto fail;

	/*
	 * Replace the program DIFO instruction buffer, BPF relocation table,
	 * and variable table with the new versions.
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

	/*
	 * Write out the new string table.
	 */
	dt_free(dtp, dp->dtdo_strtab);
	dp->dtdo_strlen = dt_strtab_size(stab);
	if (dp->dtdo_strlen > 0) {
		dp->dtdo_strtab = dt_zalloc(dtp, dp->dtdo_strlen);
		if (dp->dtdo_strtab == NULL)
			goto nomem;
		dt_strtab_write(stab, (dt_strtab_write_f *)dt_strtab_copystr,
				dp->dtdo_strtab);
	} else
		dp->dtdo_strtab = NULL;

	dt_strtab_destroy(stab);

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

	if (dt_link(dtp, prp, dp, idp) != 0) {
		dt_difo_free(dtp, dp);
		return NULL;
	}

	DT_DISASM_PROG_LINKED(dtp, cflags, dp, stderr, idp, prp->desc);

	return dp;
}

dtrace_prog_t *
dtrace_program_strcompile(dtrace_hdl_t *dtp, const char *s,
    dtrace_probespec_t spec, uint_t cflags, int argc, char *const argv[])
{
	dtrace_prog_t *rv;

	rv = dt_compile(dtp, DT_CTX_DPROG, spec, NULL, cflags,
			argc, argv, NULL, s);
	if (rv == NULL)
		return NULL;

	DT_DISASM_CLAUSE(dtp, cflags, rv, stderr);

	return rv;
}

dtrace_prog_t *
dtrace_program_fcompile(dtrace_hdl_t *dtp, FILE *fp,
    uint_t cflags, int argc, char *const argv[])
{
	dtrace_prog_t *rv;

	rv = dt_compile(dtp, DT_CTX_DPROG, DTRACE_PROBESPEC_NAME, NULL, cflags,
			argc, argv, fp, NULL);
	if (rv == NULL)
		return NULL;

	DT_DISASM_CLAUSE(dtp, cflags, rv, stderr);

	return rv;
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
