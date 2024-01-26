/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PCB_H
#define	_DT_PCB_H

#include <dtrace.h>
#include <setjmp.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include <dt_parser.h>
#include <dt_regset.h>
#include <dt_decl.h>
#include <dt_as.h>

typedef struct dt_pcb {
	dtrace_hdl_t *pcb_hdl;	/* pointer to library handle */
	struct dt_pcb *pcb_prev; /* pointer to previous pcb in stack */
	FILE *pcb_fileptr;	/* pointer to input file (or NULL) */
	char *pcb_filetag;	/* optional file name string (or NULL) */
	const char *pcb_string;	/* pointer to input string (or NULL) */
	int pcb_sargc;		/* number of script arguments (if any) */
	char *const *pcb_sargv;	/* script argument strings (if any) */
	ushort_t *pcb_sflagv;	/* script argument flags (DT_IDFLG_* bits) */
	dt_scope_t pcb_dstack;	/* declaration processing stack */
	dt_node_t *pcb_list;	/* list of allocated parse tree nodes */
	dt_node_t *pcb_hold;	/* parse tree nodes on hold until end of defn */
	dt_node_t *pcb_root;	/* root of current parse tree */
	dt_idstack_t pcb_globals; /* stack of global identifier hash tables */
	dt_idhash_t *pcb_locals; /* current hash table of local identifiers */
	dt_idhash_t *pcb_idents; /* current hash table of ambiguous idents */
	dt_idhash_t *pcb_pragmas; /* current hash table of pending pragmas */
	dt_regset_t *pcb_regs;	/* register set for code generation */
	uint32_t pcb_bufoff;	/* output buffer offset (for DFUNCs) */
	dt_irlist_t pcb_ir;	/* list of unrelocated IR instructions */
	uint_t pcb_exitlbl;	/* label for exit of program */
	uint_t pcb_asvidx;	/* assembler vartab index (see dt_as.c) */
	ulong_t **pcb_asxrefs;	/* assembler imported xlators (see dt_as.c) */
	uint_t pcb_asxreflen;	/* assembler xlator map length (see dt_as.c) */
	const dtrace_probedesc_t *pcb_pdesc; /* probedesc for current context */
	struct dt_probe *pcb_parent_probe; /* parent of PCB probe */
	struct dt_probe *pcb_probe; /* probe associated with current context */
	dtrace_probeinfo_t pcb_pinfo; /* info associated with current context */
	dtrace_datadesc_t *pcb_ddesc; /* data record description */
	int pcb_maxrecs;	/* alloc'd number of data record descriptions */
	dtrace_attribute_t pcb_amin; /* stability minimum for compilation */
	dtrace_difo_t *pcb_difo; /* intermediate DIF object made by assembler */
	dtrace_prog_t *pcb_prog; /* intermediate program made by compiler */
	dtrace_stmtdesc_t *pcb_stmt; /* intermediate stmt made by compiler */
	dtrace_ecbdesc_t *pcb_ecbdesc; /* intermediate ecbdesc made by cmplr */
	jmp_buf pcb_jmpbuf;	/* setjmp(3C) buffer for error return */
	const char *pcb_region;	/* optional region name for yyerror() suffix */
	dtrace_probespec_t pcb_pspec; /* probe description evaluation context */
	uint_t pcb_cflags;	/* optional compilation flags (see dtrace.h) */
	uint_t pcb_idepth;	/* preprocessor #include nesting depth */
	yystate_t pcb_yystate;	/* lex/yacc parsing state (see yybegin()) */
	int pcb_context;	/* yyparse() rules context (DT_CTX_* value) */
	int pcb_token;		/* token to be returned by yylex() (if != 0) */
	int pcb_cstate;		/* state to be restored by lexer at state end */
	int pcb_braces;		/* number of open curly braces in lexer */
	int pcb_brackets;	/* number of open square brackets in lexer */
	int pcb_parens;		/* number of open parentheses in lexer */
	int pcb_sou_type;	/* lexer in struct/union type name */
	int pcb_sou_deref;	/* lexer in struct/union dereference */
	int pcb_xlator_input;	/* in translator input type */
	int pcb_array_dimens;	/* in array dimensions */
	int pcb_alloca_taints;	/* number of alloca taint changes */
} dt_pcb_t;

extern void dt_pcb_push(dtrace_hdl_t *, dt_pcb_t *);
extern void dt_pcb_pop(dtrace_hdl_t *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PCB_H */
