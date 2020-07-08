/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DTRACE_H
#define	_DTRACE_H

#include <stdarg.h>
#include <stdio.h>
#include <gelf.h>
#include <sys/ctf-api.h>
#include <sys/dtrace.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * DTrace Dynamic Tracing Software: Library Interfaces
 *
 * Note: The contents of this file are private to the implementation of the
 * DTrace subsystem and are subject to change at any time without notice.
 * Applications and drivers using these interfaces will fail to run on future
 * releases.  These interfaces should not be used for any purpose except those
 * expressly outlined in dtrace(7D) and libdtrace(3LIB).  Please refer to the
 * "Solaris Dynamic Tracing Guide" for more information.
 */

#define	DTRACE_VERSION	3		/* library ABI interface version */

typedef struct dtrace_hdl dtrace_hdl_t;
typedef struct dtrace_prog dtrace_prog_t;
typedef struct dtrace_vector dtrace_vector_t;
typedef struct dtrace_aggdata dtrace_aggdata_t;

#define	DTRACE_O_LP64		0x02	/* force D compiler to be LP64 */
#define	DTRACE_O_ILP32		0x04	/* force D compiler to be ILP32 */
#define	DTRACE_O_MASK		0x07	/* mask of valid flags to dtrace_open */

extern dtrace_hdl_t *dtrace_open(int version, int flags, int *errp);
extern dtrace_hdl_t *dtrace_vopen(int version, int flags, int *errp,
    const dtrace_vector_t *vector, void *arg);

extern int dtrace_go(dtrace_hdl_t *dtp);
extern int dtrace_stop(dtrace_hdl_t *dtp);
extern void dtrace_sleep(dtrace_hdl_t *dtp);
extern void dtrace_close(dtrace_hdl_t *dtp);

extern int dtrace_errno(dtrace_hdl_t *dtp);
extern const char *dtrace_errmsg(dtrace_hdl_t *dtp, int error);
extern const char *dtrace_faultstr(dtrace_hdl_t *dtp, int fault);
extern const char *dtrace_subrstr(dtrace_hdl_t *dtp, int subr);

extern int dtrace_setopt(dtrace_hdl_t *dtp, const char *opt, const char *val);
extern int dtrace_getopt(dtrace_hdl_t *dtp, const char *opt,
    dtrace_optval_t *val);
extern void dtrace_setoptenv(dtrace_hdl_t *dtp, const char *prefix);

extern int dtrace_update(dtrace_hdl_t *dtp);
extern int dtrace_ctlfd(dtrace_hdl_t *dtp);

/*
 * DTrace Program Interface
 *
 * DTrace programs can be created by compiling ASCII text files containing
 * D programs or by compiling in-memory C strings that specify a D program.
 * Once created, callers can examine the list of program statements and
 * enable the probes and actions described by these statements.
 */

typedef enum dtrace_probespec {
	DTRACE_PROBESPEC_NONE = -1,
	DTRACE_PROBESPEC_PROVIDER = 0,
	DTRACE_PROBESPEC_MOD,
	DTRACE_PROBESPEC_FUNC,
	DTRACE_PROBESPEC_NAME
} dtrace_probespec_t;

typedef struct dtrace_proginfo {
	dtrace_attribute_t dpi_descattr; /* minimum probedesc attributes */
	dtrace_attribute_t dpi_stmtattr; /* minimum statement attributes */
	uint_t dpi_aggregates;	/* number of aggregates specified in program */
	uint_t dpi_recgens;	/* number of record generating probes in prog */
	uint_t dpi_matches;	/* number of probes matched by program */
	uint_t dpi_speculations; /* number of speculations specified in prog */
} dtrace_proginfo_t;

#define	DTRACE_C_DIFV	0x0001	/* DIF verbose mode: show each compiled DIFO */
#define	DTRACE_C_EMPTY	0x0002	/* Permit compilation of empty D source files */
#define	DTRACE_C_ZDEFS	0x0004	/* Permit probe defs that match zero probes */
#define	DTRACE_C_EATTR	0x0008	/* Error if program attributes less than min */
#define	DTRACE_C_CPP	0x0010	/* Preprocess input file with cpp(1) utility */
#define	DTRACE_C_KNODEF	0x0020	/* Permit unresolved kernel symbols in DIFO */
#define	DTRACE_C_UNODEF	0x0040	/* Permit unresolved user symbols in DIFO */
#define	DTRACE_C_PSPEC  0x0080	/* Interpret ambiguous specifiers as probes */
#define	DTRACE_C_ETAGS	0x0100	/* Prefix error messages with error tags */
#define	DTRACE_C_ARGREF	0x0200	/* Do not require all macro args to be used */
#define	DTRACE_C_DEFARG	0x0800	/* Use 0/"" as value for unspecified args */
#define	DTRACE_C_NOLIBS	0x1000	/* Do not process D system libraries */
#define	DTRACE_C_CTL	0x2000	/* Only process control directives */
#define	DTRACE_C_MASK	0x3bff	/* mask of all valid flags to dtrace_*compile */

extern dtrace_prog_t *dtrace_program_strcompile(dtrace_hdl_t *dtp, const char *s,
    dtrace_probespec_t spec, uint_t cflags, int argc, char *const argv[]);

extern dtrace_prog_t *dtrace_program_fcompile(dtrace_hdl_t *dtp, FILE *fp,
    uint_t cflags, int argc, char *const argv[]);

extern int dtrace_program_exec(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_proginfo_t *pip);
extern void dtrace_program_info(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_proginfo_t *pip);

#define	DTRACE_D_STRIP	0x01	/* strip non-loadable sections from program */
#define	DTRACE_D_PROBES	0x02	/* include provider and probe definitions */
#define	DTRACE_D_MASK	0x03	/* mask of valid flags to dtrace_dof_create */

extern int dtrace_program_link(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    uint_t dflags, const char *file, int objc, char *const objv[]);

extern int dtrace_program_header(dtrace_hdl_t *dtp, FILE *out,
    const char *fname);

extern void *dtrace_dof_create(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    uint_t flags);
extern void dtrace_dof_destroy(dtrace_hdl_t *dtp, void *dof);

extern void *dtrace_getopt_dof(dtrace_hdl_t *dtp);
extern void *dtrace_geterr_dof(dtrace_hdl_t *dtp);

typedef struct dtrace_stmtdesc {
	dtrace_ecbdesc_t *dtsd_ecbdesc;		/* ECB description */
	dtrace_actdesc_t *dtsd_action;		/* action list */
	dtrace_datadesc_t *dtsd_ddesc;		/* probe data description */
	void *dtsd_aggdata;			/* aggregation data */
	void *dtsd_fmtdata;			/* type-specific output data */
	void (*dtsd_callback)();		/* callback function for EPID */
	void *dtsd_data;			/* callback data pointer */
	dtrace_attribute_t dtsd_descattr;	/* probedesc attributes */
	dtrace_attribute_t dtsd_stmtattr;	/* statement attributes */
	int dtsd_padding;			/* work around GCC bug 36043 */
} dtrace_stmtdesc_t;

typedef int dtrace_stmt_f(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_stmtdesc_t *sdp, void *data);

extern dtrace_stmtdesc_t *dtrace_stmt_create(dtrace_hdl_t *dtp,
    dtrace_ecbdesc_t *edp);
extern dtrace_actdesc_t *dtrace_stmt_action(dtrace_hdl_t *dtp,
    dtrace_stmtdesc_t *sdp);
extern int dtrace_stmt_add(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_stmtdesc_t *sdp);
extern int dtrace_stmt_iter(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_stmt_f *sdp, void *data);
extern void dtrace_stmt_destroy(dtrace_hdl_t *dtp, dtrace_stmtdesc_t *sdp);

/*
 * DTrace Data Consumption Interface
 */
typedef enum {
	DTRACEFLOW_ENTRY,
	DTRACEFLOW_RETURN,
	DTRACEFLOW_NONE
} dtrace_flowkind_t;

#define	DTRACE_CONSUME_ERROR		-1	/* error while processing */
#define	DTRACE_CONSUME_THIS		0	/* consume this probe/record */
#define	DTRACE_CONSUME_NEXT		1	/* advance to next probe/rec */
#define	DTRACE_CONSUME_ABORT		2	/* abort consumption */

typedef struct dtrace_probedata {
	dtrace_hdl_t *dtpda_handle;		/* handle to DTrace library */
	dtrace_epid_t dtpda_epid;		/* enabled probe ID */
	dtrace_datadesc_t *dtpda_ddesc;		/* probe data description */
	dtrace_probedesc_t *dtpda_pdesc;	/* probe description */
	unsigned int dtpda_cpu;			/* CPU for data */
	caddr_t dtpda_data;			/* pointer to raw data */
	dtrace_flowkind_t dtpda_flow;		/* flow kind */
	const char *dtpda_prefix;		/* recommended flow prefix */
	int dtpda_indent;			/* recommended flow indent */
} dtrace_probedata_t;

typedef int dtrace_consume_probe_f(const dtrace_probedata_t *data, void *arg);
typedef int dtrace_consume_rec_f(const dtrace_probedata_t *data,
    const dtrace_recdesc_t *rec, void *arg);

extern int dtrace_consume(dtrace_hdl_t *dtp, FILE *fp,
    dtrace_consume_probe_f *pf, dtrace_consume_rec_f *rf, void *arg);

#define	DTRACE_STATUS_NONE	0	/* no status; not yet time */
#define	DTRACE_STATUS_OKAY	1	/* status okay */
#define	DTRACE_STATUS_EXITED	2	/* exit() was called; tracing stopped */
#define	DTRACE_STATUS_FILLED	3	/* fill buffer filled; tracing stoped */
#define	DTRACE_STATUS_STOPPED	4	/* tracing already stopped */

extern int dtrace_status(dtrace_hdl_t *dtp);

/*
 * DTrace returns the results of a single tracemem() invocation in either one or
 * two consecutive DTRACEACT_TRACEMEM records.  The first contains the buffer
 * itself;  the second, if present, contains the value of a dynamic size
 * specified by the user.  The records may be differentiated by their arguments,
 * which take the following possible values:
 */
#define	DTRACE_TRACEMEM_STATIC	1	/* buffer; no dynamic size */
#define	DTRACE_TRACEMEM_DYNAMIC	2	/* buffer; dynamic size follows */
#define	DTRACE_TRACEMEM_SIZE	3	/* dynamic size (unsigned) */
#define	DTRACE_TRACEMEM_SSIZE	4	/* dynamic size (signed) */

/*
 * DTrace Formatted Output Interfaces
 *
 * To format output associated with a given dtrace_stmtdesc, the caller can
 * invoke one of the following functions, passing the opaque dtsd_fmtdata and a
 * list of record descriptions.  These functions return either -1 to indicate
 * an error, or a positive integer indicating the number of records consumed.
 * For anonymous enablings, the consumer can use the dtrd_format member of
 * the record description to obtain a format description.  The dtfd_string
 * member of the format description may be passed to dtrace_print{fa}_create()
 * to create the opaque format data.
 */
extern void *dtrace_printf_create(dtrace_hdl_t *dtp, const char *s);
extern void *dtrace_printa_create(dtrace_hdl_t *dtp, const char *s);
extern size_t dtrace_printf_format(dtrace_hdl_t *dtp, void *fmtdata, char *s,
    size_t len);

extern int dtrace_fprintf(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
    const dtrace_probedata_t *data, const dtrace_recdesc_t *recp,
    uint_t nrecs, const void *buf, size_t len);

extern int dtrace_fprinta(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
    const dtrace_probedata_t *data, const dtrace_recdesc_t *recs,
    uint_t nrecs, const void *buf, size_t len);

extern int dtrace_system(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
    const dtrace_probedata_t *data, const dtrace_recdesc_t *recp,
    uint_t nrecs, const void *buf, size_t len);

extern int dtrace_freopen(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
    const dtrace_probedata_t *data, const dtrace_recdesc_t *recp,
    uint_t nrecs, const void *buf, size_t len);

/*
 * DTrace Work Interface
 */
typedef enum {
	DTRACE_WORKSTATUS_ERROR = -1,
	DTRACE_WORKSTATUS_OKAY,
	DTRACE_WORKSTATUS_DONE
} dtrace_workstatus_t;

extern dtrace_workstatus_t dtrace_work(dtrace_hdl_t *dtp, FILE *fp,
    dtrace_consume_probe_f *pfunc, dtrace_consume_rec_f *rfunc, void *arg);

/*
 * DTrace Handler Interface
 */
#define	DTRACE_HANDLE_ABORT		-1	/* abort current operation */
#define	DTRACE_HANDLE_OK		0	/* handled okay; continue */

typedef struct dtrace_errdata {
	dtrace_hdl_t *dteda_handle;		/* handle to DTrace library */
	dtrace_datadesc_t *dteda_ddesc;		/* probe data inducing err */
	dtrace_probedesc_t *dteda_pdesc;	/* probe inducing error */
	unsigned int dteda_cpu;			/* CPU of error */
	int dteda_action;			/* action inducing error */
	int dteda_offset;			/* offset in DIFO of error */
	int dteda_fault;			/* specific fault */
	uint64_t dteda_addr;			/* address of fault, if any */
	const char *dteda_msg;			/* preconstructed message */
} dtrace_errdata_t;

typedef int dtrace_handle_err_f(const dtrace_errdata_t *err, void *arg);
extern int dtrace_handle_err(dtrace_hdl_t *dtp, dtrace_handle_err_f *hdlr,
    void *arg);

typedef enum {
	DTRACEDROP_PRINCIPAL,			/* drop to principal buffer */
	DTRACEDROP_AGGREGATION,			/* drop to aggregation buffer */
	DTRACEDROP_DYNAMIC,			/* dynamic drop */
	DTRACEDROP_DYNRINSE,			/* dyn drop due to rinsing */
	DTRACEDROP_DYNDIRTY,			/* dyn drop due to dirty */
	DTRACEDROP_SPEC,			/* speculative drop */
	DTRACEDROP_SPECBUSY,			/* spec drop due to busy */
	DTRACEDROP_SPECUNAVAIL,			/* spec drop due to unavail */
	DTRACEDROP_STKSTROVERFLOW,		/* stack string tab overflow */
	DTRACEDROP_DBLERROR			/* error in ERROR probe */
} dtrace_dropkind_t;

typedef struct dtrace_dropdata {
	dtrace_hdl_t *dtdda_handle;		/* handle to DTrace library */
	unsigned int dtdda_cpu;			/* CPU, if any */
	dtrace_dropkind_t dtdda_kind;		/* kind of drop */
	uint64_t dtdda_drops;			/* number of drops */
	uint64_t dtdda_total;			/* total drops */
	const char *dtdda_msg;			/* preconstructed message */
} dtrace_dropdata_t;

typedef int dtrace_handle_drop_f(const dtrace_dropdata_t *drop, void *arg);
extern int dtrace_handle_drop(dtrace_hdl_t *dtp, dtrace_handle_drop_f *hdlr,
    void *arg);

typedef void dtrace_handle_proc_f(pid_t pid, const char *err, void *arg);
extern int dtrace_handle_proc(dtrace_hdl_t *dtp, dtrace_handle_proc_f *hdlr,
    void *arg);

#define	DTRACE_BUFDATA_AGGKEY		0x0001	/* aggregation key */
#define	DTRACE_BUFDATA_AGGVAL		0x0002	/* aggregation value */
#define	DTRACE_BUFDATA_AGGFORMAT	0x0004	/* aggregation format data */
#define	DTRACE_BUFDATA_AGGLAST		0x0008	/* last for this key/val */

typedef struct dtrace_bufdata {
	dtrace_hdl_t *dtbda_handle;		/* handle to DTrace library */
	const char *dtbda_buffered;		/* buffered output */
	dtrace_probedata_t *dtbda_probe;	/* probe data */
	const dtrace_recdesc_t *dtbda_recdesc;	/* record description */
	const dtrace_aggdata_t *dtbda_aggdata;	/* aggregation data, if agg. */
	uint32_t dtbda_flags;			/* flags; see above */
} dtrace_bufdata_t;

typedef int dtrace_handle_buffered_f(const dtrace_bufdata_t *data, void *arg);
extern int dtrace_handle_buffered(dtrace_hdl_t *dtp,
    dtrace_handle_buffered_f *hdlr, void *arg);

typedef struct dtrace_setoptdata {
	dtrace_hdl_t *dtsda_handle;		/* handle to DTrace library */
	const dtrace_probedata_t *dtsda_probe;	/* probe data */
	const char *dtsda_option;		/* option that was set */
	dtrace_optval_t dtsda_oldval;		/* old value */
	dtrace_optval_t dtsda_newval;		/* new value */
} dtrace_setoptdata_t;

typedef int dtrace_handle_setopt_f(const dtrace_setoptdata_t *data, void *arg);
extern int dtrace_handle_setopt(dtrace_hdl_t *dtp,
    dtrace_handle_setopt_f *hdlr, void *arg);

/*
 * DTrace Aggregate Interface
 */

#define	DTRACE_A_PERCPU		0x0001
#define	DTRACE_A_KEEPDELTA	0x0002
#define	DTRACE_A_ANONYMOUS	0x0004

#define	DTRACE_AGGWALK_ERROR		-1	/* error while processing */
#define	DTRACE_AGGWALK_NEXT		0	/* proceed to next element */
#define	DTRACE_AGGWALK_ABORT		1	/* abort aggregation walk */
#define	DTRACE_AGGWALK_CLEAR		2	/* clear this element */
#define	DTRACE_AGGWALK_NORMALIZE	3	/* normalize this element */
#define	DTRACE_AGGWALK_DENORMALIZE	4	/* denormalize this element */
#define	DTRACE_AGGWALK_REMOVE		5	/* remove this element */

struct dtrace_aggdata {
	dtrace_hdl_t *dtada_handle;		/* handle to DTrace library */
	dtrace_aggdesc_t *dtada_desc;		/* aggregation description */
	dtrace_datadesc_t *dtada_ddesc;		/* probe data description */
	dtrace_probedesc_t *dtada_pdesc;	/* probe description */
	caddr_t dtada_data;			/* pointer to raw data */
	uint64_t dtada_normal;			/* the normal -- 1 for denorm */
	size_t dtada_size;			/* total size of the data */
	caddr_t dtada_delta;			/* delta data, if available */
	caddr_t *dtada_percpu;			/* per CPU data, if avail */
	caddr_t *dtada_percpu_delta;		/* per CPU delta, if avail */
};

typedef struct dtrace_print_aggdata {
	dtrace_hdl_t *dtpa_dtp;			/* handle to DTrace library */
	dtrace_aggvarid_t dtpa_id;		/* aggregation variable */
	FILE *dtpa_fp;				/* file pointer */
	int dtpa_allunprint;			/* print only unprinted aggs */
} dtrace_print_aggdata_t;

typedef int dtrace_aggregate_f(const dtrace_aggdata_t *func, void *arg);
typedef int dtrace_aggregate_walk_f(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);
typedef int dtrace_aggregate_walk_joined_f(const dtrace_aggdata_t **aggvars,
    const int naggvars, void *arg);

extern void dtrace_aggregate_clear(dtrace_hdl_t *dtp);
extern int dtrace_aggregate_snap(dtrace_hdl_t *dtp);
extern int dtrace_aggregate_print(dtrace_hdl_t *dtp, FILE *fp,
    dtrace_aggregate_walk_f *func); /* func must take dtrace_print_aggdata_t */

extern int dtrace_aggregate_walk(dtrace_hdl_t *dtp, dtrace_aggregate_f *func,
    void *arg);

extern int dtrace_aggregate_walk_joined(dtrace_hdl_t *dtp,
    dtrace_aggvarid_t *aggvars, int naggvars,
    dtrace_aggregate_walk_joined_f *func, void *arg);

extern int dtrace_aggregate_walk_sorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

extern int dtrace_aggregate_walk_keysorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

extern int dtrace_aggregate_walk_valsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

extern int dtrace_aggregate_walk_keyvarsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

extern int dtrace_aggregate_walk_valvarsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

extern int dtrace_aggregate_walk_keyrevsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

extern int dtrace_aggregate_walk_valrevsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

extern int dtrace_aggregate_walk_keyvarrevsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

extern int dtrace_aggregate_walk_valvarrevsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg);

#define	DTRACE_AGD_PRINTED	0x1	/* aggregation printed in program */

/*
 * DTrace Process Control Interface
 *
 * Library clients who wish to have libdtrace create or grab processes for
 * monitoring of their symbol table changes may use these interfaces to
 * request that libdtrace obtain control of the process using libproc.
 */

/*
 * If this bit is set in flags, an automatic dtrace_proc_continue() is
 * performed before returning from dtrace_proc_{create|grab}().
 */
#define DTRACE_PROC_WAITING 0x01

/*
 * If this bit is set, this is a relatively unimportant and short-lived grab:
 * the system can sacrifice accuracy for performance and safety, minimizing the
 * use of ptrace() and possibly not stopping the process: failing to ptrace() is
 * nonfatal.
 */
#define DTRACE_PROC_SHORTLIVED 0x02

/*
 * Handle to a process.
 */
struct dtrace_proc;

extern struct dtrace_proc *dtrace_proc_create(dtrace_hdl_t *dtp,
    const char *file, char *const *argv, int flags);

extern struct dtrace_proc *dtrace_proc_grab_pid(dtrace_hdl_t *dtp,
    pid_t pid, int flags);
extern void dtrace_proc_release(dtrace_hdl_t *dtp,
    struct dtrace_proc *proc);
extern void dtrace_proc_continue(dtrace_hdl_t *dtp,
    struct dtrace_proc *proc);
extern pid_t dtrace_proc_getpid(dtrace_hdl_t *dtp,
    struct dtrace_proc *proc);

/*
 * DTrace Object, Symbol, and Type Interfaces
 *
 * Library clients can use libdtrace to perform symbol and C type information
 * lookups by symbol name, symbol address, or C type name, or to lookup meta-
 * information cached for each of the program objects in use by DTrace.  The
 * resulting struct contains pointers to address range arrays and arbitrary-
 * length strings, including object, symbol, and type names, that are persistent
 * until the next call to dtrace_update().  Once dtrace_update() is called, any
 * cached values must be flushed and not used subsequently by the client
 * program.
 */

#define	DTRACE_OBJ_EXEC	 ((const char *)0L)	/* primary executable file */
#define	DTRACE_OBJ_CDEFS ((const char *)1L)	/* C include definitions */
#define	DTRACE_OBJ_DDEFS ((const char *)2L)	/* D program definitions */
#define	DTRACE_OBJ_EVERY ((const char *)-1L)	/* all known objects */
#define	DTRACE_OBJ_KMODS ((const char *)-2L)	/* all kernel objects */
#define	DTRACE_OBJ_UMODS ((const char *)-3L)	/* all user objects */

typedef struct dtrace_addr_range {
	GElf_Addr dar_va;	/* virtual start address of range */
	GElf_Xword dar_size;	/* size of range */
} dtrace_addr_range_t;

extern int dtrace_addr_range_cmp(const void *addr, const void *range);

typedef struct dtrace_objinfo {
	const char *dto_name;			/* object file scope name */
	const char *dto_file;			/* object file path (if any) */
	uint_t dto_flags;			/* object flags (see below) */
	dtrace_addr_range_t *dto_text_addrs;	/* text addresses, sorted */
	size_t dto_text_addrs_size;		/* number of entries */
	dtrace_addr_range_t *dto_data_addrs;	/* data/BSS addresses, sorted */
	size_t dto_data_addrs_size;		/* number of entries */
} dtrace_objinfo_t;

#define	DTRACE_OBJ_F_KERNEL	0x1		/* object is a kernel module */

typedef int dtrace_obj_f(dtrace_hdl_t *dtp, const dtrace_objinfo_t *modinfo,
    void *data);

extern int dtrace_object_iter(dtrace_hdl_t *dtp, dtrace_obj_f *func,
    void *data);
extern int dtrace_object_info(dtrace_hdl_t *dtp, const char *object,
    dtrace_objinfo_t *dto);

typedef struct dtrace_syminfo {
	const char *object;			/* object name */
	const char *name;			/* symbol name */
	ulong_t id;				/* symbol id */
} dtrace_syminfo_t;

extern int dtrace_lookup_by_name(dtrace_hdl_t *dtp, const char *object,
    const char *name, GElf_Sym *symp, dtrace_syminfo_t *sip);

extern int dtrace_lookup_by_addr(dtrace_hdl_t *dtp, GElf_Addr addr,
    GElf_Sym *symp, dtrace_syminfo_t *sip);

typedef struct dtrace_typeinfo {
	const char *dtt_object;			/* object containing type */
	ctf_file_t *dtt_ctfp;			/* CTF container handle */
	ctf_id_t dtt_type;			/* CTF type identifier */
} dtrace_typeinfo_t;

extern int dtrace_lookup_by_type(dtrace_hdl_t *dtp, const char *object,
    const char *name, dtrace_typeinfo_t *tip);

extern int dtrace_symbol_type(dtrace_hdl_t *dtp, const GElf_Sym *symp,
    const dtrace_syminfo_t *sip, dtrace_typeinfo_t *tip);

extern int dtrace_type_strcompile(dtrace_hdl_t *dtp, const char *s,
    dtrace_typeinfo_t *dtt);

extern int dtrace_type_fcompile(dtrace_hdl_t *dtp, FILE *fp,
    dtrace_typeinfo_t *dtt);

/*
 * DTrace Probe Interface
 *
 * Library clients can use these functions to iterate over the set of available
 * probe definitions and inquire as to their attributes.  The probe iteration
 * interfaces report probes that are declared as well as those from dtrace(7D).
 */
typedef struct dtrace_probeinfo {
	dtrace_attribute_t dtp_attr;		/* name attributes */
	dtrace_attribute_t dtp_arga;		/* arg attributes */
	const dtrace_typeinfo_t *dtp_argv;	/* arg types */
	int dtp_argc;				/* arg count */
} dtrace_probeinfo_t;

typedef int dtrace_probe_f(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pd,
    void *arg);

extern int dtrace_probe_iter(dtrace_hdl_t *dtp,
    const dtrace_probedesc_t *pdp, dtrace_probe_f *func, void *arg);

extern int dtrace_probe_info(dtrace_hdl_t *dtp,
    const dtrace_probedesc_t *pdp, dtrace_probeinfo_t *pip);

/*
 * DTrace Vector Interface
 *
 * The DTrace library normally speaks directly to dtrace(7D).  However,
 * this communication may be vectored elsewhere.  Consumers who wish to
 * perform a vectored open must fill in the vector, and use the dtrace_vopen()
 * entry point to obtain a library handle.
 */
struct dtrace_vector {
	int (*dtv_ioctl)(void *varg, int val, void *arg);
	int (*dtv_lookup_by_addr)(void *varg, GElf_Addr addr, GElf_Sym *symp,
	    dtrace_syminfo_t *sip);
	int (*dtv_cpu_status)(void *varg, int cpu);
	long (*dtv_sysconf)(void *varg, int name);
};

/*
 * DTrace Utility Functions
 *
 * Library clients can use these functions to convert addresses strings, to
 * convert between string and integer probe descriptions and the
 * dtrace_probedesc_t representation, and to perform similar conversions on
 * stability attributes.
 */
extern int dtrace_addr2str(dtrace_hdl_t *dtp, uint64_t addr, char *str,
    int nbytes);
extern int dtrace_uaddr2str(dtrace_hdl_t *dtp, pid_t pid,
    uint64_t addr, char *str, int nbytes);

extern int dtrace_xstr2desc(dtrace_hdl_t *dtp, dtrace_probespec_t spec,
    const char *s, int argc, char *const argv[], dtrace_probedesc_t *pdp);

extern int dtrace_str2desc(dtrace_hdl_t *dtp, dtrace_probespec_t spec,
    const char *s, dtrace_probedesc_t *pdp);

extern int dtrace_id2desc(dtrace_hdl_t *dtp, dtrace_id_t id,
    dtrace_probedesc_t *pdp);

#define	DTRACE_DESC2STR_MAX	1024	/* min buf size for dtrace_desc2str() */

extern char *dtrace_desc2str(const dtrace_probedesc_t *pdp, char *buf,
    size_t len);

#define	DTRACE_ATTR2STR_MAX	64	/* min buf size for dtrace_attr2str() */

extern char *dtrace_attr2str(dtrace_attribute_t attr, char *buf, size_t len);
extern int dtrace_str2attr(const char *str, dtrace_attribute_t *attr);

extern const char *dtrace_stability_name(dtrace_stability_t s);
extern const char *dtrace_class_name(dtrace_class_t c);

extern int dtrace_provider_modules(dtrace_hdl_t *dtp, const char **mods,
    int nmods);

extern const char *const _dtrace_version;
extern const char *const _libdtrace_vcs_version;
extern int _dtrace_debug;

extern void dtrace_debug_set_dump_sig(int signal);

#ifdef	__cplusplus
}
#endif

#endif	/* _DTRACE_H */
