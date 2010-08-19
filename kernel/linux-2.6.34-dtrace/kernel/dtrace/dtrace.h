#ifndef _DTRACE_H_
#define _DTRACE_H_

#include <linux/cred.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/stringify.h>
#include <asm/bitsperlong.h>
#include <asm/ptrace.h>

#define UINT16_MAX		(0xffff)
#define UINT16_MIN		0
#define UINT32_MAX		(0xffffffff)
#define UINT32_MIN		0
#define UINT64_MAX		(~0ULL)
#define UINT64_MIN		(0)

#define NBBY			(__BITS_PER_LONG / sizeof (long))

#define DIF_TYPE_CTF		0
#define DIF_TYPE_STRING		1

#define DIF_VAR_OTHER_MIN	0x0100
#define DIF_VAR_OTHER_UBASE	0x0500
#define DIF_VAR_OTHER_MAX	0xffff

#define DIF_VAR_ARGS		0x0000
#define DIF_VAR_REGS		0x0001
#define DIF_VAR_UREGS		0x0002
#define DIF_VAR_CURTHREAD	0x0100
#define DIF_VAR_TIMESTAMP	0x0101
#define DIF_VAR_VTIMESTAMP	0x0102
#define DIF_VAR_IPL		0x0103
#define DIF_VAR_EPID		0x0104
#define DIF_VAR_ID		0x0105
#define DIF_VAR_ARG0		0x0106
#define DIF_VAR_ARG1		0x0107
#define DIF_VAR_ARG2		0x0108
#define DIF_VAR_ARG3		0x0109
#define DIF_VAR_ARG4		0x010a
#define DIF_VAR_ARG5		0x010b
#define DIF_VAR_ARG6		0x010c
#define DIF_VAR_ARG7		0x010d
#define DIF_VAR_ARG8		0x010e
#define DIF_VAR_ARG9		0x010f
#define DIF_VAR_STACKDEPTH	0x0110
#define DIF_VAR_CALLER		0x0111
#define DIF_VAR_PROBEPROV	0x0112
#define DIF_VAR_PROBEMOD	0x0113
#define DIF_VAR_PROBEFUNC	0x0114
#define DIF_VAR_PROBENAME	0x0115
#define DIF_VAR_PID		0x0116
#define DIF_VAR_TID		0x0117
#define DIF_VAR_EXECNAME	0x0118
#define DIF_VAR_ZONENAME	0x0119
#define DIF_VAR_WALLTIMESTAMP	0x011a
#define DIF_VAR_USTACKDEPTH	0x011b
#define DIF_VAR_UCALLER		0x011c
#define DIF_VAR_PPID		0x011d
#define DIF_VAR_UID		0x011e
#define DIF_VAR_GID		0x011f
#define DIF_VAR_ERRNO		0x0120

#define DIF_TF_BYREF		0x1

#define DIFV_KIND_ARRAY		0
#define DIFV_KIND_SCALAR	1

#define DIFV_SCOPE_GLOBAL	0
#define DIFV_SCOPE_THREAD	1
#define DIFV_SCOPE_LOCAL	2

#define DIFV_F_REF		0x1
#define DIFV_F_MOD		0x2

#define DTRACE_IDNONE		0

#define DTRACE_PROVNAMELEN	64
#define DTRACE_MODNAMELEN	64
#define DTRACE_FUNCNAMELEN	128
#define DTRACE_NAMELEN		64

#define DTRACE_ARGTYPELEN	128

#define DTRACE_PROBEKEY_MAXDEPTH	8

#define DTRACE_STABILITY_INTERNAL	0
#define DTRACE_STABILITY_PRIVATE	1
#define DTRACE_STABILITY_OBSOLETE	2
#define DTRACE_STABILITY_EXTERNAL	3
#define DTRACE_STABILITY_UNSTABLE	4
#define DTRACE_STABILITY_EVOLVING	5
#define DTRACE_STABILITY_STABLE		6
#define DTRACE_STABILITY_STANDARD	7
#define DTRACE_STABILITY_MAX		7

#define DTRACE_CLASS_UNKNOWN	0
#define DTRACE_CLASS_CPU	1
#define DTRACE_CLASS_PLATFORM	2
#define DTRACE_CLASS_GROUP	3
#define DTRACE_CLASS_ISA	4
#define DTRACE_CLASS_COMMON	5
#define DTRACE_CLASS_MAX	5

#define DTRACE_COND_OWNER	0x01
#define DTRACE_COND_USERMODE	0x02

#define DTRACE_CRV_ALLPROC	0x01
#define DTRACE_CRV_KERNEL	0x02
#define DTRACE_DRV_ALL		(DTRACE_CRV_ALLPROC | DTRACE_CRV_KERNEL)

#define DTRACE_MATCH_FAIL	-1
#define DTRACE_MATCH_NEXT	0
#define DTRACE_MATCH_DONE	1

#define DTRACE_PRIV_NONE	0x0000
#define DTRACE_PRIV_KERNEL	0x0001
#define DTRACE_PRIV_USER	0x0002
#define DTRACE_PRIV_PROC	0x0004
#define DTRACE_PRIV_OWNER	0x0008
#define DTRACE_PRIV_ALL		(DTRACE_PRIV_KERNEL | DTRACE_PRIV_USER | \
				 DTRACE_PRIV_PROC | DTRACE_PRIV_OWNER)

#define DTRACE_QUANTIZE_NBUCKETS		\
		(((sizeof (uint64_t) * NBBY) - 1) * 2 + 1)

#define DTRACE_QUANTIZE_ZEROBUCKET	((sizeof (uint64_t) * NBBY) - 1)

#define DTRACE_QUANTIZE_BUCKETVAL(buck)		\
	(int64_t)((buck) < DTRACE_QUANTIZE_ZEROBUCKET ? \
		  -(1LL << (DTRACE_QUANTIZE_ZEROBUCKET - 1 - (buck))) : \
		  (buck) == DTRACE_QUANTIZE_ZEROBUCKET ? 0 : \
		  1LL << ((buck) - DTRACE_QUANTIZE_ZEROBUCKET - 1))

#define DTRACE_LQUANTIZE_STEPSHIFT	48
#define DTRACE_LQUANTIZE_STEPMASK	((uint64_t)UINT16_MAX << 48)
#define DTRACE_LQUANTIZE_LEVELSHIFT	32
#define DTRACE_LQUANTIZE_LEVELMASK	((uint64_t)UINT16_MAX << 32)
#define DTRACE_LQUANTIZE_BASESHIFT	0
#define DTRACE_LQUANTIZE_BASEMASK	UINT32_MAX

#define DTRACE_LQUANTIZE_STEP(x)		\
		(uint16_t)(((x) & DTRACE_LQUANTIZE_STEPMASK) >> \
			   DTRACE_LQUANTIZE_STEPSHIFT)

#define DTRACE_LQUANTIZE_LEVELS(x)		\
		(uint16_t)(((x) & DTRACE_LQUANTIZE_LEVELMASK) >> \
			   DTRACE_LQUANTIZE_LEVELSHIFT)

#define DTRACE_LQUANTIZE_BASE(x)		\
		(int32_t)(((x) & DTRACE_LQUANTIZE_BASEMASK) >> \
			  DTRACE_LQUANTIZE_BASESHIFT)

#define DTRACE_USTACK_NFRAMES(x)	(uint32_t)((x) & UINT32_MAX)
#define DTRACE_USTACK_STRSIZE(x)	(uint32_t)((x) >> 32)
#define DTRACE_USTACK_ARG(x, y)		\
		((((uint64_t)(y)) << 32) | ((x) & UINT32_MAX))

#define DTRACEACT_NONE			0
#define DTRACEACT_DIFEXPR		1
#define DTRACEACT_EXIT			2
#define DTRACEACT_PRINTF		3
#define DTRACEACT_PRINTA		4
#define DTRACEACT_LIBACT		5

#define DTRACEACT_PROC			0x0100
#define DTRACEACT_USTACK		(DTRACEACT_PROC + 1)
#define DTRACEACT_JSTACK		(DTRACEACT_PROC + 2)
#define DTRACEACT_USYM			(DTRACEACT_PROC + 3)
#define DTRACEACT_UMOD			(DTRACEACT_PROC + 4)
#define DTRACEACT_UADDR			(DTRACEACT_PROC + 5)

#define DTRACEACT_PROC_DESTRUCTIVE	0x0200
#define DTRACEACT_STOP			(DTRACEACT_PROC_DESTRUCTIVE + 1)
#define DTRACEACT_RAISE			(DTRACEACT_PROC_DESTRUCTIVE + 2)
#define DTRACEACT_SYSTEM		(DTRACEACT_PROC_DESTRUCTIVE + 3)
#define DTRACEACT_FREOPEN		(DTRACEACT_PROC_DESTRUCTIVE + 4)

#define DTRACEACT_PROC_CONTROL		0x0300

#define DTRACEACT_KERNEL		0x0400
#define DTRACEACT_STACK			(DTRACEACT_KERNEL + 1)
#define DTRACEACT_SYM			(DTRACEACT_KERNEL + 2)
#define DTRACEACT_MOD			(DTRACEACT_KERNEL + 3)

#define DTRACEACT_KERNEL_DESTRUCTIVE	0x0500
#define DTRACEACT_BREAKPOINT		(DTRACEACT_KERNEL_DESTRUCTIVE + 1)
#define DTRACEACT_PANIC			(DTRACEACT_KERNEL_DESTRUCTIVE + 2)
#define DTRACEACT_CHILL			(DTRACEACT_KERNEL_DESTRUCTIVE + 3)

#define DTRACEACT_SPECULATIVE           0x0600
#define DTRACEACT_SPECULATE		(DTRACEACT_SPECULATIVE + 1)
#define DTRACEACT_COMMIT		(DTRACEACT_SPECULATIVE + 2)
#define DTRACEACT_DISCARD		(DTRACEACT_SPECULATIVE + 3)

#define DTRACEACT_AGGREGATION		0x0700
#define DTRACEAGG_COUNT			(DTRACEACT_AGGREGATION + 1)
#define DTRACEAGG_MIN			(DTRACEACT_AGGREGATION + 2)
#define DTRACEAGG_MAX			(DTRACEACT_AGGREGATION + 3)
#define DTRACEAGG_AVG			(DTRACEACT_AGGREGATION + 4)
#define DTRACEAGG_SUM			(DTRACEACT_AGGREGATION + 5)
#define DTRACEAGG_STDDEV		(DTRACEACT_AGGREGATION + 6)
#define DTRACEAGG_QUANTIZE		(DTRACEACT_AGGREGATION + 7)
#define DTRACEAGG_LQUANTIZE		(DTRACEACT_AGGREGATION + 8)

#define DTRACEACT_CLASS(x)		((x) & 0xff00)

#define DTRACEACT_ISAGG(x)		\
		(DTRACEACT_CLASS(x) == DTRACEACT_AGGREGATION)

#define DTRACEACT_ISDESTRUCTIVE(x)	\
		(DTRACEACT_CLASS(x) == DTRACEACT_PROC_DESTRUCTIVE || \
		 DTRACEACT_CLASS(x) == DTRACEACT_KERNEL_DESTRUCTIVE)

#define DTRACEACT_ISSPECULATIVE(x)	\
		(DTRACEACT_CLASS(x) == DTRACEACT_SPECULATIVE)

#define DTRACEACT_ISPRINTFLIKE(x)	\
		((x) == DTRACEACT_PRINTF || (x) == DTRACEACT_PRINTA || \
		 (x) == DTRACEACT_SYSTEM || (x) == DTRACEACT_FREOPEN)

#define DTRACEOPT_BUFSIZE	0
#define DTRACEOPT_BUFPOLICY	1
#define DTRACEOPT_DYNVARSIZE	2
#define DTRACEOPT_AGGSIZE	3
#define DTRACEOPT_SPECSIZE	4
#define DTRACEOPT_NSPEC		5
#define DTRACEOPT_STRSIZE	6
#define DTRACEOPT_CLEANRATE	7
#define DTRACEOPT_CPU		8
#define DTRACEOPT_BUFRESIZE	9
#define DTRACEOPT_GRABANON	10
#define DTRACEOPT_FLOWINDENT	11
#define DTRACEOPT_QUIET		12
#define DTRACEOPT_STACKFRAMES	13
#define DTRACEOPT_USTACKFRAMES	14
#define DTRACEOPT_AGGRATE	15
#define DTRACEOPT_SWITCHRATE	16
#define DTRACEOPT_STATUSRATE	17
#define DTRACEOPT_DESTRUCTIVE	18
#define DTRACEOPT_STACKINDENT	19
#define DTRACEOPT_RAWBYTES	20
#define DTRACEOPT_JSTACKFRAMES	21
#define DTRACEOPT_JSTACKSTRSIZE	22
#define DTRACEOPT_AGGSORTKEY	23
#define DTRACEOPT_AGGSORTREV	24
#define DTRACEOPT_AGGSORTPOS	25
#define DTRACEOPT_AGGSORTKEYPOS	26
#define DTRACEOPT_MAX		27

#define DTRACEOPT_UNSET		(dtrace_optval_t)-2

#define DTRACEOPT_BUFPOLICY_RING	0
#define DTRACEOPT_BUFPOLICY_FULL	1
#define DTRACEOPT_BUFPOLICY_SWITCH	2

#define DTRACEOPT_BUFRESIZE_AUTO	0
#define DTRACEOPT_BUFRESIZE_MANUAL	1

typedef uint8_t		dtrace_stability_t;
typedef uint8_t		dtrace_class_t;

typedef uint16_t	dtrace_actkind_t;

typedef uint32_t	zoneid_t;	/* FIXME */
typedef uint32_t	dif_instr_t;
typedef uint32_t	dtrace_aggid_t;
typedef uint32_t	dtrace_cacheid_t;
typedef uint32_t	dtrace_epid_t;
typedef uint32_t	uint_t;

typedef uint64_t	dtrace_genid_t;
typedef uint64_t	dtrace_optval_t;

typedef enum {
	TRUE = -1,
	FALSE = 0
} boolean_t;

typedef struct cred	cred_t;
typedef struct hrtimer	hrtime_t;

typedef typeof(((struct pt_regs *)0)->ip)	pc_t;

typedef void		*cyclic_id_t;	/* FIXME */

#define P2ROUNDUP(x, a)	(-(-(x) & -(a)))

typedef struct dtrace_ppriv {
	uint32_t dtpp_flags;
	uid_t dtpp_uid;
	zoneid_t dtpp_zoneid;
} dtrace_ppriv_t;

typedef struct dtrace_attribute {
	dtrace_stability_t dtat_name;
	dtrace_stability_t dtat_data;
	dtrace_class_t dtat_class;
} dtrace_attribute_t;

typedef struct dtrace_pattr {
	dtrace_attribute_t dtpa_provider;
	dtrace_attribute_t dtpa_mod;
	dtrace_attribute_t dtpa_func;
	dtrace_attribute_t dtpa_name;
	dtrace_attribute_t dtpa_args;
} dtrace_pattr_t;

typedef uint32_t dtrace_id_t;

typedef struct dtrace_probedesc {
	dtrace_id_t dtpd_id;
	char dtpd_provider[DTRACE_PROVNAMELEN];
	char dtpd_mod[DTRACE_MODNAMELEN];
	char dtpd_func[DTRACE_FUNCNAMELEN];
	char dtpd_name[DTRACE_NAMELEN];
} dtrace_probedesc_t;

typedef struct dtrace_argdesc {
	dtrace_id_t dtargd_id;
	int dtargd_ndx;
	int dtargd_mapping;
	char dtargd_native[DTRACE_ARGTYPELEN];
	char dtargd_xlate[DTRACE_ARGTYPELEN];
} dtrace_argdesc_t;

typedef struct dtrace_pops {
	void (*dtps_provide)(void *, const dtrace_probedesc_t *);
	void (*dtps_provide_module)(void *, struct module *);
	int (*dtps_enable)(void *, dtrace_id_t, void *);
	void (*dtps_disable)(void *, dtrace_id_t, void *);
	void (*dtps_suspend)(void *, dtrace_id_t, void *);
	void (*dtps_resume)(void *, dtrace_id_t, void *);
	void (*dtps_getargdesc)(void *, dtrace_id_t, void *,
				dtrace_argdesc_t *);
	uint64_t (*dtps_getargval)(void *, dtrace_id_t, void *, int, int);
	int (*dtps_usermode)(void *, dtrace_id_t, void *);
	void (*dtps_destroy)(void *, dtrace_id_t, void *);
} dtrace_pops_t;

typedef struct dtrace_helper_probedesc {
	char *dthpb_mod;
	char *dthpb_func;
	char *dthpb_name;
	uint64_t dthpb_base;
	uint32_t *dthpb_offs;
	uint32_t *dthpb_enoffs;
	uint32_t dthpb_noffs;
	uint32_t dthpb_nenoffs;
	uint8_t *dthpb_args;
	uint8_t dthpb_xargc;
	uint8_t dthpb_nargc;
	char *dthpb_xtypes;
	char *dthpb_ntypes;
} dtrace_helper_probedesc_t;

typedef struct dtrace_helper_provdesc {
	char *dthpv_provname;
	dtrace_pattr_t dthpv_pattr;
} dtrace_helper_provdesc_t;

typedef struct dtrace_mops {
	void (*dtms_create_probe)(void *, void *, dtrace_helper_probedesc_t *);
	void (*dtms_provide_pid)(void *, dtrace_helper_provdesc_t *, pid_t);
	void (*dtms_remove_pid)(void *, dtrace_helper_provdesc_t *, pid_t);
} dtrace_mops_t;

typedef struct dtrace_provider {
	dtrace_pattr_t dtpv_attr;
	dtrace_ppriv_t dtpv_priv;
	dtrace_pops_t dtpv_pops;
	char *dtpv_name;
	void *dtpv_arg;
	uint_t dtpv_defunct;
	struct dtrace_provider *dtpv_next;
} dtrace_provider_t;

typedef struct dtrace_diftype {
	uint8_t dtdt_kind;
	uint8_t dtdt_ckind;
	uint8_t dtdt_flags;
	uint8_t dtdt_pad;
	uint32_t dtdt_size;
} dtrace_diftype_t;

typedef struct dtrace_difv {
	uint32_t dtdv_name;
	uint32_t dtdv_id;
	uint8_t dtdv_kind;
	uint8_t dtdv_scope;
	uint16_t dtdv_flags;
	dtrace_diftype_t dtdv_type;
} dtrace_difv_t;

typedef struct dtrace_difo {
	dif_instr_t *dtdo_buf;
	uint64_t *dtdo_inttab;
	char *dtdo_strtab;
	dtrace_difv_t *dtdo_vartab;
	uint_t dtdo_len;
	uint_t dtdo_intlen;
	uint_t dtdo_strlen;
	uint_t dtdo_varlen;
	dtrace_diftype_t dtdo_rtype;
	uint_t dtdo_refcnt;
	uint_t dtdo_destructive;
#ifndef __KERNEL__
	dof_relodesc_t *dtdo_kreltab;
	dof_relodesc_t *dtdo_ureltab;
	struct dt_node **dtdo_xlmtab;
	uint_t dtdo_krelen;
	uint_t dtdo_urelen;
	uint_t dtdo_xlmlen;
#endif
} dtrace_difo_t;

typedef struct dtrace_actdesc {
	dtrace_difo_t *dtad_difo;
	struct dtrace_actdesc *dtad_next;
	dtrace_actkind_t dtad_kind;
	uint32_t dtad_ntuple;
	uint64_t dtad_arg;
	uint64_t dtad_uarg;
	int dtad_refcnt;
} dtrace_actdesc_t;

typedef struct dtrace_predicate {
	dtrace_difo_t *dtp_difo;
	dtrace_cacheid_t dtp_cacheid;
	int dtp_refcnt;
} dtrace_predicate_t;

typedef struct dtrace_preddesc {
	dtrace_difo_t *dtpdd_difo;
	dtrace_predicate_t *dtpdd_predicate;
} dtrace_preddesc_t;

typedef struct dtrace_ecbdesc {
	dtrace_actdesc_t *dted_action;
	dtrace_preddesc_t dted_pred;
	dtrace_probedesc_t dted_probe;
	uint64_t dted_uarg;
	int dted_refcnt;
} dtrace_ecbdesc_t;

typedef struct dtrace_statvar {
	uint64_t dtsv_data;
	size_t dtsv_size;
	int dtsv_refcnt;
	dtrace_difv_t dtsv_var;
} dtrace_statvar_t;

typedef struct dtrace_recdesc {
	dtrace_actkind_t dtrd_action;
	uint32_t dtrd_size;
	uint32_t dtrd_offset;
	uint16_t dtrd_alignment;
	uint16_t dtrd_format;
	uint64_t dtrd_arg;
	uint64_t dtrd_uarg;
} dtrace_recdesc_t;

typedef struct dtrace_action {
	dtrace_actkind_t dta_kind;
	uint16_t dta_intuple;
	uint32_t dta_refcnt;
	dtrace_difo_t *dta_difo;
	dtrace_recdesc_t dta_rec;
	struct dtrace_action *dta_prev;
	struct dtrace_action *dta_next;
} dtrace_action_t;

struct dtrace_ecb;
typedef struct dtrace_ecb	dtrace_ecb_t;

typedef struct dtrace_probe {
	dtrace_id_t dtpr_id;
	dtrace_ecb_t *dtpr_ecb;
	dtrace_ecb_t *dtpr_ecb_last;
	void *dtpr_arg;
	dtrace_cacheid_t dtpr_predcache;
	int dtpr_aframes;
	dtrace_provider_t *dtpr_provider;
	char *dtpr_mod;
	char *dtpr_func;
	char *dtpr_name;
	struct dtrace_probe *dtpr_nextmod;
	struct dtrace_probe *dtpr_prevmod;
	struct dtrace_probe *dtpr_nextfunc;
	struct dtrace_probe *dtpr_prevfunc;
	struct dtrace_probe *dtpr_nextname;
	struct dtrace_probe *dtpr_prevname;
	dtrace_genid_t dtpr_gen;
} dtrace_probe_t;

struct dtrace_state;
typedef struct dtrace_state	dtrace_state_t;

struct dtrace_ecb {
	dtrace_epid_t dte_epid;
	uint32_t dte_alignment;
	size_t dte_needed;
	size_t dte_size;
	dtrace_predicate_t *dte_predicate;
	dtrace_action_t *dte_action;
	struct dtrace_ecb *dte_next;
	dtrace_state_t *dte_state;
	uint32_t dte_cond;
	dtrace_probe_t *dte_probe;
	dtrace_action_t *dte_action_last;
	uint64_t dte_uarg;
};

typedef enum dtrace_activity {
	DTRACE_ACTIVITY_INACTIVE = 0,
	DTRACE_ACTIVITY_WARMUP,
	DTRACE_ACTIVITY_ACTIVE,
	DTRACE_ACTIVITY_DRAINING,
	DTRACE_ACTIVITY_COOLDOWN,
	DTRACE_ACTIVITY_STOPPED,
	DTRACE_ACTIVITY_KILLED
} dtrace_activity_t;

typedef enum dtrace_dstate_state {
	DTRACE_DSTATE_CLEAN = 0,
	DTRACE_DSTATE_EMPTY,
	DTRACE_DSTATE_DIRTY,
	DTRACE_DSTATE_RINSING
} dtrace_dstate_state_t;

typedef struct dtrace_key {
	uint64_t dttk_value;
	uint64_t dttk_size;
} dtrace_key_t;

typedef struct dtrace_tuple {
	uint32_t dtt_nkeys;
	uint32_t dtt_pad;
	dtrace_key_t dtt_key[1];
} dtrace_tuple_t;

typedef struct dtrace_dynvar {
	uint64_t dtdv_hashval;
	struct dtrace_dynvar *dtdv_next;
	void *dtdv_data;
	dtrace_tuple_t dtdv_tuple;
} dtrace_dynvar_t;

typedef struct dtrace_dstate_percpu {
	dtrace_dynvar_t *dtdsc_free;
	dtrace_dynvar_t *dtdsc_dirty;
	dtrace_dynvar_t *dtdsc_rinsing;
	dtrace_dynvar_t *dtdsc_clean;
	uint64_t dtdsc_drops;
	uint64_t dtdsc_dirty_drops;
	uint64_t dtdsc_rinsing_drops;
#ifdef _LP64
	uint64_t dtdsc_pad;
#else
	uint64_t dtdsc_pad[2];
#endif
} dtrace_dstate_percpu_t;

typedef struct dtrace_dynhash {
	dtrace_dynvar_t *dtdh_chain;
	uintptr_t dtdh_lock;
#ifdef _LP64
	uintptr_t dtdh_pad[6];
#else
	uintptr_t dtdh_pad[14];
#endif
} dtrace_dynhash_t;

typedef struct dtrace_dstate {
	void *dtds_base;
	size_t dtds_size;
	size_t dtds_hashsize;
	size_t dtds_chunksize;
	dtrace_dynhash_t *dtds_hash;
	dtrace_dstate_state_t dtds_state;
	dtrace_dstate_percpu_t *dtds_percpu;
} dtrace_dstate_t;

typedef struct dtrace_vstate {
	dtrace_state_t *dtvs_state;
	dtrace_statvar_t **dtvs_globals;
	int dtvs_nglobals;
	dtrace_difv_t *dtvs_tlocals;
	int dtvs_ntlocals;
	dtrace_statvar_t **dtvs_locals;
	int dtvs_nlocals;
	dtrace_dstate_t dtvs_dynvars;
} dtrace_vstate_t;

typedef struct dtrace_buffer {
	uint64_t dtb_offset;
	uint64_t dtb_size;
	uint32_t dtb_flags;
	uint32_t dtb_drops;
	caddr_t dtb_tomax;
	caddr_t dtb_xamot;
	uint32_t dtb_xamot_flags;
	uint32_t dtb_xamot_drops;
	uint64_t dtb_xamot_offset;
	uint32_t dtb_errors;
	uint32_t dtb_xamot_errors;
#ifndef _LP64
	uint64_t dtb_pad1;
#endif
} dtrace_buffer_t;

typedef enum dtrace_speculation_state {
	DTRACESPEC_INACTIVE = 0,
	DTRACESPEC_ACTIVE,
	DTRACESPEC_ACTIVEONE,
	DTRACESPEC_ACTIVEMANY,
	DTRACESPEC_COMMITTING,
	DTRACESPEC_COMMITTINGMANY,
	DTRACESPEC_DISCARDING
} dtrace_speculation_state_t;

typedef struct dtrace_speculation {
	dtrace_speculation_state_t dtsp_state;
	int dtsp_cleaning;
	dtrace_buffer_t *dtsp_buffer;
} dtrace_speculation_t;

typedef struct dtrace_aggregation {
	dtrace_action_t dtag_action;
	dtrace_aggid_t dtag_id;
	dtrace_ecb_t *dtag_ecb;
	dtrace_action_t *dtag_first;
	uint32_t dtag_base;
	uint8_t dtag_hasarg;
	uint64_t dtag_initial;
	void (*dtag_aggregate)(uint64_t *, uint64_t, uint64_t);
} dtrace_aggregation_t;

typedef struct dtrace_cred {
	cred_t *dcr_cred;
	uint8_t dcr_destructive;
	uint8_t dcr_visible;
	uint16_t dcr_action;
} dtrace_cred_t;

struct dtrace_state {
	dev_t dts_dev;
	int dts_necbs;
	dtrace_ecb_t **dts_ecbs;
	dtrace_epid_t dts_epid;
	size_t dts_needed;
	struct dtrace_state *dts_anon;
	dtrace_activity_t dts_activity;
	dtrace_vstate_t dts_vstate;
	dtrace_buffer_t *dts_buffer;
	dtrace_buffer_t *dts_aggbuffer;
	dtrace_speculation_t *dts_speculations;
	int dts_nspeculations;
	int dts_naggregations;
	dtrace_aggregation_t **dts_aggregations;
	void *dts_aggid_arena;	/* NOT USED / FIXME: was vmem_t * */
	uint64_t dts_errors;
	uint32_t dts_speculations_busy;
	uint32_t dts_speculations_unavail;
	uint32_t dts_stkstroverflows;
	uint32_t dts_dblerrors;
	uint32_t dts_reserve;
	hrtime_t dts_laststatus;
	cyclic_id_t dts_cleaner;
	cyclic_id_t dts_deadman;
	hrtime_t dts_alive;
	char dts_speculates;
	char dts_destructive;
	int dts_nformats;
	char **dts_formats;
	dtrace_optval_t dts_options[DTRACEOPT_MAX];
	dtrace_cred_t dts_cred;
	size_t dts_nretained;
};

typedef struct dtrace_enabling {
	dtrace_ecbdesc_t **dten_desc;
	int dten_ndesc;
	int dten_maxdesc;
	dtrace_vstate_t *dten_vstate;
	dtrace_genid_t dten_probegen;
	dtrace_ecbdesc_t *dten_current;
	int dten_error;
	int dten_primed;
	struct dtrace_enabling *dten_prev;
	struct dtrace_enabling *dten_next;
} dtrace_enabling_t;

typedef int dtrace_probekey_f(const char *, const char *, int);

typedef struct dtrace_probekey {
	const char *dtpk_prov;
	dtrace_probekey_f *dtpk_pmatch;
	const char *dtpk_mod;
	dtrace_probekey_f *dtpk_mmatch;
	const char *dtpk_func;
	dtrace_probekey_f *dtpk_fmatch;
	const char *dtpk_name;
	dtrace_probekey_f *dtpk_nmatch;
	dtrace_id_t dtpk_id;
} dtrace_probekey_t;

typedef struct dtrace_hashbucket {
	struct dtrace_hashbucket *dthb_next;
	dtrace_probe_t *dthb_chain;
	int dthb_len;
} dtrace_hashbucket_t;

typedef struct dtrace_hash {
	dtrace_hashbucket_t **dth_tab;
	int dth_size;
	int dth_mask;
	int dth_nbuckets;
	uintptr_t dth_nextoffs;
	uintptr_t dth_prevoffs;
	uintptr_t dth_stroffs;
} dtrace_hash_t;

extern struct mutex	dtrace_lock;
extern struct mutex	dtrace_provider_lock;
extern struct mutex	dtrace_meta_lock;

extern dtrace_genid_t	dtrace_probegen;

/*
 * DTrace Probe Context Functions
 */
extern void dtrace_aggregate_min(uint64_t *, uint64_t, uint64_t);
extern void dtrace_aggregate_max(uint64_t *, uint64_t, uint64_t);
extern void dtrace_aggregate_quantize(uint64_t *, uint64_t, uint64_t);
extern void dtrace_aggregate_lquantize(uint64_t *, uint64_t, uint64_t);
extern void dtrace_aggregate_avg(uint64_t *, uint64_t, uint64_t);
extern void dtrace_aggregate_stddev(uint64_t *, uint64_t, uint64_t);
extern void dtrace_aggregate_count(uint64_t *, uint64_t, uint64_t);
extern void dtrace_aggregate_sum(uint64_t *, uint64_t, uint64_t);

/*
 * DTrace Probe Hashing Functions
 */
#define DTRACE_HASHNEXT(hash, probe)	\
	(dtrace_probe_t **)((uintptr_t)(probe) + (hash)->dth_nextoffs)

extern dtrace_probe_t *dtrace_hash_lookup(dtrace_hash_t *, dtrace_probe_t *);
extern int dtrace_hash_collisions(dtrace_hash_t *, dtrace_probe_t *);

/*
 * DTrace Matching Function
 */
extern int dtrace_match_priv(const dtrace_probe_t *, uint32_t, uid_t);
extern int dtrace_match_probe(const dtrace_probe_t *,
			      const dtrace_probekey_t *, uint32_t, uid_t);
extern int dtrace_match(const dtrace_probekey_t *, uint32_t, uid_t,
			int (*matched)(dtrace_probe_t *, void *), void *arg);
extern void dtrace_probekey(const dtrace_probedesc_t *, dtrace_probekey_t *);

/*
 * DTrace Provider-to-Framework API Functions
 */
typedef uintptr_t	dtrace_provider_id_t;
typedef uintptr_t	dtrace_meta_provider_id_t;

extern dtrace_provider_t	*dtrace_provider;

extern int dtrace_register(const char *, const dtrace_pattr_t *, uint32_t,
			   cred_t *, const dtrace_pops_t *, void *,
			   dtrace_provider_id_t *);
extern int dtrace_unregister(dtrace_provider_id_t);

extern int dtrace_meta_register(const char *, const dtrace_mops_t *, void *,
				dtrace_meta_provider_id_t *);
extern int dtrace_meta_unregister(dtrace_meta_provider_id_t);

/*
 * DTrace Probe Management Functions
 */
extern int		dtrace_nprobes;
extern dtrace_probe_t	**dtrace_probes;

extern int dtrace_probe_enable(const dtrace_probedesc_t *,
			       dtrace_enabling_t *);
extern void dtrace_probe_provide(dtrace_probedesc_t *, dtrace_provider_t *);
extern dtrace_probe_t *dtrace_probe_lookup_id(dtrace_id_t);

/*
 * DTrace DIF Object Functions
 */
extern void dtrace_difo_hold(dtrace_difo_t *);
extern void dtrace_difo_release(dtrace_difo_t *, dtrace_vstate_t *);

/*
 * DTrace Format Functions
 */
extern uint16_t dtrace_format_add(dtrace_state_t *, char *);
extern void dtrace_format_remove(dtrace_state_t *, uint16_t);

/*
 * DTrace Predicate Functions
 */
extern void dtrace_predicate_hold(dtrace_predicate_t *);
extern void dtrace_predicate_release(dtrace_predicate_t *, dtrace_vstate_t *);

/*
 * DTrace ECB Functions
 */
extern dtrace_ecb_t	*dtrace_ecb_create_cache;

extern int dtrace_ecb_create_enable(dtrace_probe_t *, void *);
extern void dtrace_ecb_destroy(dtrace_ecb_t *);
extern void dtrace_ecb_resize(dtrace_ecb_t *);
extern int dtrace_ecb_enable(dtrace_ecb_t *);

/*
 * DTrace Enabling Functions
 */
extern dtrace_enabling_t	*dtrace_retained;
extern dtrace_genid_t		dtrace_retained_gen;

extern void dtrace_enabling_matchall(void);
extern void dtrace_enabling_provide(dtrace_provider_t *);

/*
 * DTrace Utility Functions
 */
extern int dtrace_badattr(const dtrace_attribute_t *);
extern int dtrace_badname(const char *);
extern void dtrace_cred2priv(cred_t *, uint32_t *, uid_t *);

#define DT_PROVIDER_MODULE(name)					\
  static dtrace_provider_id_t name##_id;				\
									\
  static int __init name##_init(void)					\
  {									\
	int ret = 0;							\
									\
	ret = name##_dev_init();					\
	if (ret)							\
		goto failed;						\
									\
	ret = dtrace_register(__stringify(name), &name##_attr,		\
			      DTRACE_PRIV_USER, NULL, &name##_pops,	\
			      NULL, &name##_id);			\
	if (ret)							\
		goto failed;						\
									\
	return 0;							\
									\
  failed:								\
	return ret;							\
  }									\
									\
  static void __exit name##_exit(void)					\
  {									\
	dtrace_unregister(name##_id);					\
	name##_dev_exit();						\
  }									\
									\
  module_init(name##_init);						\
  module_exit(name##_exit);

extern void dtrace_sync(void);
extern void dtrace_vtime_enable(void);
extern void dtrace_vtime_disable(void);

#endif /* _DTRACE_H_ */
