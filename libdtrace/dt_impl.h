/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_IMPL_H
#define	_DT_IMPL_H

#include <sys/param.h>
#include <setjmp.h>
#include <sys/ctf-api.h>
#include <dtrace.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/dtrace_types.h>
#include <sys/utsname.h>
#include <sys/compiler.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <bpf_asm.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#include <dt_bpf_maps.h>
#include <dt_parser.h>
#include <dt_regset.h>
#include <dt_rodata.h>
#include <dt_strtab.h>
#include <dt_symtab.h>
#include <dt_ident.h>
#include <dt_htab.h>
#include <dt_list.h>
#include <dt_decl.h>
#include <dt_as.h>
#include <dt_proc.h>
#include <dt_pcap.h>
#include <dt_dof.h>
#include <dt_pcb.h>
#include <dt_printf.h>
#include <dt_debug.h>
#include <dt_version.h>

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef __stringify
# define __stringify_(x)         #x
# define __stringify(x)          __stringify_(x)
#endif

/*
 * Note that string = "" is an empty string:  its first byte is 0x00.
 * To store a NULL string -- basically, (char*)0 --  make the first byte 0x00.
 * Then, to distinguish between empty and NULL strings, follow the initial
 * byte either with 0s (empty string) or a special set of nonzero bytes.
 * That is, to designate a NULL string, make the initial bytes DT_NULL_STRING,
 * where:
 *   - the initial byte is 0x00
 *   - subsequent bytes are nonzero
 *   - the width is at least 2 bytes
 *      (that is, there is at least one nonzero byte)
 *   - the width is at most 4 bytes
 *      (so that DT_NULL_STRING can be used as an IMM)
 *   - the highest bit is 0
 *      (so that if DT_NULL_STRING is an IMM, it won't get sign extended)
 * Finally, note that strsize must be large enough to hold DT_NULL_STRING.
 */
#ifdef _BIG_ENDIAN
#define DT_NULL_STRING ((uint16_t)0x007f)
#else
#define DT_NULL_STRING ((uint16_t)0x7f00)
#endif

struct dt_module;		/* see below */
struct dt_pfdict;		/* see <dt_printf.h> */
struct dt_arg;			/* see below */
struct dt_provider;		/* see <dt_provider.h> */
struct dt_probe;		/* see <dt_probe.h> */
struct pid_probespec;		/* see <pid.h> */
struct dt_pebset;		/* see <dt_peb.h> */
struct dt_xlator;		/* see <dt_xlator.h> */

typedef struct dt_intrinsic {
	const char *din_name;	/* string name of the intrinsic type */
	ctf_encoding_t din_data; /* integer or floating-point CTF encoding */
	uint_t din_kind;	/* CTF type kind to instantiate */
} dt_intrinsic_t;

typedef struct dt_typedef {
	const char *dty_src;	/* string name of typedef source type */
	const char *dty_dst;	/* string name of typedef destination type */
} dt_typedef_t;

typedef struct dt_intdesc {
	const char *did_name;	/* string name of the integer type */
	ctf_file_t *did_ctfp;	/* CTF container for this type reference */
	ctf_id_t did_type;	/* CTF type reference for this type */
	uintmax_t did_limit;	/* maximum positive value held by type */
} dt_intdesc_t;

typedef struct dt_modops {
	uint_t (*do_syminit)(struct dt_module *);
	void (*do_symsort)(struct dt_module *);
	GElf_Sym *(*do_symname)(struct dt_module *,
	    const char *, GElf_Sym *, uint_t *);
	GElf_Sym *(*do_symaddr)(struct dt_module *,
	    GElf_Addr, GElf_Sym *, uint_t *);
} dt_modops_t;

typedef struct dt_arg {
	int da_ndx;		/* index of this argument */
	int da_mapping;		/* mapping of argument indices to arguments */
	ctf_id_t da_type;	/* type of argument */
	ctf_file_t *da_ctfp;	/* CTF container for type */
	dt_ident_t *da_xlator;	/* translator, if any */
	struct dt_arg *da_next;	/* next argument */
} dt_arg_t;

typedef struct dt_modsym {
	uint_t dms_symid;	/* id of corresponding symbol */
	uint_t dms_next;	/* index of next element in hash chain */
} dt_modsym_t;

typedef struct dt_module {
	dt_list_t dm_list;	/* list forward/back pointers */
	char dm_name[DTRACE_MODNAMELEN]; /* string name of module */
	char dm_file[PATH_MAX]; /* file path of module */
	uint_t dm_flags;	/* module flags (see below) */
	struct dt_hentry dm_he;	/* htab links */
	dtrace_hdl_t *dm_dtp;	/* backpointer to the dtrace instance */


	dtrace_addr_range_t *dm_text_addrs; /* text addresses, sorted */
	size_t dm_text_addrs_size;	 /* number of entries */
	dtrace_addr_range_t *dm_data_addrs; /* data/BSS addresses, sorted */
	size_t dm_data_addrs_size;	 /* number of entries */

	dt_idhash_t *dm_extern;	/* external symbol definitions */

	/*
	 * Kernel modules populate the fields below only for the sake of CTF.
	 * When a CTF archive is available, most are not populated at all.
	 */
	const dt_modops_t *dm_ops; /* pointer to data model's ops vector */
	Elf *dm_elf;		/* libelf handle for module object */
	ctf_file_t *dm_ctfp;	/* CTF container handle */
	void *dm_ctdata_data;	/* CTF section data (dynamically allocated) */
	ctf_sect_t dm_ctdata;	/* CTF data for module */
	ctf_sect_t dm_symtab;	/* symbol table */
	ctf_sect_t dm_strtab;	/* string table */

	/*
	 * Kernel modules only.
	 */
	dt_symtab_t *dm_kernsyms; /* module kernel symbol table */

	/*
	 * Userspace modules only.
	 */
	uint_t *dm_symbuckets;	/* symbol table hash buckets (chain indices) */
	dt_modsym_t *dm_symchains; /* symbol table hash chains buffer */
	void *dm_asmap;		/* symbol pointers sorted by value */
	uint_t dm_symfree;	/* index of next free hash element */
	uint_t dm_nsymbuckets;	/* number of elements in bucket array */
	uint_t dm_nsymelems;	/* number of elements in hash table */
	uint_t dm_asrsv;	/* actual reserved size of dm_asmap */
	uint_t dm_aslen;	/* number of entries in dm_asmap */
} dt_module_t;

/*
 * The path to an actual kernel module residing on the disk.  Used only for
 * initialization of the corresponding dt_module, since modprobe -l is
 * deprecated (and removed in the kmod tools).
 */
typedef struct dt_kern_path {
	dt_list_t dkp_list;	       /* list forward/back pointers */
	struct dt_hentry dkp_he;       /* htab links */
	char *dkp_name;		       /* name of this kernel module */
	char *dkp_path;		       /* full name including path */
} dt_kern_path_t;

#define DT_DM_LOADED		0x1	/* module symbol and type data is loaded */
#define DT_DM_KERNEL		0x2	/* module is associated with a kernel object */
#define DT_DM_CTF_ARCHIVED	0x4	/* module found in a CTF archive */
#define DT_DM_KERN_UNLOADED	0x8	/* module not loaded into the kernel */

typedef struct dt_ahashent {
	struct dt_ahashent *dtahe_prev;		/* prev on hash chain */
	struct dt_ahashent *dtahe_next;		/* next on hash chain */
	struct dt_ahashent *dtahe_prevall;	/* prev on list of all */
	struct dt_ahashent *dtahe_nextall;	/* next on list of all */
	uint64_t dtahe_hval;			/* hash value */
	dtrace_aggdata_t dtahe_data;		/* data */
} dt_ahashent_t;

typedef struct dt_ahash {
	dt_ahashent_t	**dtah_hash;		/* hash table */
	dt_ahashent_t	*dtah_all;		/* list of all elements */
	size_t		dtah_size;		/* size of hash table */
} dt_ahash_t;

#define DT_AGGDATA_COUNTER	0
#define DT_AGGDATA_RECORD	1

/*
 * Why do we need (only) 4 slots?  The maximum amount of string arguments to
 * any function is 2, and if the result is a string as well, that means we may
 * need 3 temporary strings during code generation for that function.
 *
 * Since string functions can be nested, we can (at most) end up with 1 tstring
 * (from a nested function for which we already generated code) along with a
 * nested function being processed which needs 3 temporary strings as mentioned
 * above.  That brings us to a total of 4.
 *
 * Each tstring needs to be large enough to hold the largest possible string
 * and accomodate the largest known need for tstring space in subroutines.
 */
#define DT_TSTRING_SLOTS	4
#define DT_TSTRING_SIZE(dtp)	\
		MAX(P2ROUNDUP((dtp)->dt_options[DTRACEOPT_STRSIZE] + 1, 8), \
		    72)

typedef struct dt_tstring {
	uint64_t	offset;			/* Offset from dctx->mem */
	int		in_use;			/* In use (1) or not (0) */
} dt_tstring_t;

typedef struct dt_aggregate {
	char *dtat_key;			/* aggregation key */
	char *dtat_buf;			/* aggregation data buffer */
	char *dtat_nextkey;		/* next aggregation key */
	int dtat_flags;			/* aggregate flags */
	dt_ahash_t dtat_hash;		/* aggregate hash table */
} dt_aggregate_t;

typedef struct dt_dirpath {
	dt_list_t dir_list;		/* linked-list forward/back pointers */
	char *dir_path;			/* directory pathname */
} dt_dirpath_t;

typedef struct dt_lib_depend {
	dt_list_t dtld_deplist;		/* linked-list forward/back pointers */
	char *dtld_library;		/* library name */
	char *dtld_libpath;		/* library pathname */
	uint_t dtld_finish;		/* completion time in tsort for lib */
	uint_t dtld_start;		/* starting time in tsort for lib */
	uint_t dtld_loaded;		/* boolean: is this library loaded */
	dt_list_t dtld_dependencies;	/* linked-list of lib dependencies */
	dt_list_t dtld_dependents;	/* linked-list of lib dependents */
} dt_lib_depend_t;

typedef struct dt_spec_buf_data {
	dt_list_t dsbd_list;		/* linked-list forward/back pointers */
	unsigned int dsbd_cpu;		/* cpu for data */
	char *dsbd_data;		/* data for later processing */
	uint32_t dsbd_size;		/* size of data */
} dt_spec_buf_data_t;

typedef struct dt_spec_buf {
	dtrace_hdl_t *dtsb_dtp;		/* backpointer to the dtrace instance */
	int32_t dtsb_id;		/* speculation ID */
	size_t dtsb_size;		/* size of all buffers in this spec */
	int dtsb_committing;		/* when draining, nonzero if commit */
	dt_bpf_specs_t dtsb_spec;	/* bpf-side specs record for this spec
					   (buffer read/write counts).  */
	dt_list_t dtsb_dsbd_list;	/* list of dt_spec_bufs */
	struct dt_hentry dtsb_he;	/* htab links */
} dt_spec_buf_t;

typedef struct dt_percpu_drops {
	uint64_t	buf;		/* principal buffer drops */
	uint64_t	agg;		/* aggregate buffer drops */
} dt_percpu_drops_t;

/*
 * This will be raised much higher in future: right now it is nailed low
 * because the search-for-free-speculation code is unrolled rather than being a
 * proper loop, due to limitations in the BPF verifier.
 */
#define DT_MAX_NSPECS 16		/* sanity upper bound on speculations */

typedef uint32_t dt_version_t;		/* encoded version (see below) */

struct dtrace_hdl {
	const dtrace_vector_t *dt_vector; /* library vector, if vectored open */
	void *dt_varg;	/* vector argument, if vectored open */
	dtrace_conf_t dt_conf;	/* DTrace driver configuration profile */
	char dt_errmsg[BUFSIZ];	/* buffer for formatted syntax error msgs */
	const char *dt_errtag;	/* tag used with last call to dt_set_errmsg() */
	dt_pcb_t *dt_pcb;	/* pointer to current parsing control block */
	ulong_t dt_gen;		/* compiler generation number */
	uint_t dt_clause_nextid; /* next ID to use for programs */
	dt_list_t dt_programs;	/* linked list of dtrace_prog_t's */
	dt_list_t dt_xlators;	/* linked list of dt_xlator_t's */
	dt_list_t dt_enablings;	/* list of (to be) enabled probes */
	struct dt_xlator **dt_xlatormap; /* dt_xlator_t's indexed by dx_id */
	id_t dt_xlatorid;	/* next dt_xlator_t id to assign */
	dt_ident_t *dt_externs;	/* linked list of external symbol identifiers */
	dt_idhash_t *dt_macros;	/* hash table of macro variable identifiers */
	dt_idhash_t *dt_aggs;	/* hash table of aggregation identifiers */
	dt_idhash_t *dt_globals; /* hash table of global identifiers */
	dt_idhash_t *dt_tls;	/* hash table of thread-local identifiers */
	dt_idhash_t *dt_bpfsyms;/* hash table of BPF identifiers */
	dt_strtab_t *dt_ccstab;	/* global string table (during compilation) */
	dt_rodata_t *dt_rodata;	/* global read-only data */
	uint_t dt_strlen;	/* global string table (runtime) size */
	uint_t dt_rooffset;	/* read-only data offset */
	uint_t dt_rosize;	/* read-only data size */
	uint_t dt_zerooffset;	/* zero region, offset */
	uint_t dt_zerosize;	/* zero region, size */
	uint_t dt_maxreclen;	/* largest record size across programs */
	uint_t dt_maxdvarsize;	/* largest dynamic variable across programs */
	uint_t dt_maxtuplesize;	/* largest tuple across programs */
	uint_t dt_maxlvaralloc;	/* largest lvar alloc across pcbs */
	uint_t dt_maxaggdsize;	/* largest aggregation data sizw */
	dt_tstring_t *dt_tstrings; /* temporary string slots */
	dt_list_t dt_modlist;	/* linked list of dt_module_t's */
	dt_htab_t *dt_mods;	/* hash table of dt_module_t's */
	dt_htab_t *dt_kernsyms; /* htab of kernel symbol names */
	ctf_archive_t *dt_ctfa; /* ctf archive for the entire kernel tree */
	ctf_file_t *dt_shared_ctf; /* Handle to the shared CTF */
	char *dt_ctfa_path;	/* path to vmlinux.ctfa */
	dt_htab_t *dt_kernpaths; /* hash table of dt_kern_path_t's */
	dt_module_t *dt_exec;	/* pointer to executable module */
	dt_module_t *dt_cdefs;	/* pointer to C dynamic type module */
	dt_module_t *dt_ddefs;	/* pointer to D dynamic type module */

	/*
	 * Hashtables for quick probe lookup based on potentially partly
	 * specified probe names.
	 */
	dt_htab_t *dt_byprv;	/* htab of probes by provider name */
	dt_htab_t *dt_bymod;	/* htab of probes by module name */
	dt_htab_t *dt_byfun;	/* htab of probes by function name */
	dt_htab_t *dt_byprb;	/* htab of probes by probe name */
	dt_htab_t *dt_byfqn;	/* htab of probes by fully qualified name */

	/*
	 * Array of all known probes, to facilitate probe lookup by probe id.
	 */
	struct dt_probe **dt_probes; /* array of probes */
	size_t dt_probes_sz;	/* size of array of probes */
	uint32_t dt_probe_id;	/* next available probe id */

	/*
	 * uprobes potentially of interest: some may be instantiated as
	 * dtrace probes.
	 */
	struct pid_probespec *dt_uprobespecs;
	size_t dt_uprobespecs_sz; /* size of array of uprobes */

	struct dt_probe *dt_error; /* ERROR probe */

	dt_htab_t *dt_provs;	/* hash table of dt_provider_t's */
	const struct dt_provider *dt_prov_pid; /* PID provider */
	const struct dt_provider *dt_prov_usdt; /* USDT provider */
	dt_proc_hash_t *dt_procs; /* hash table of grabbed process handles */
	dt_intdesc_t dt_ints[6]; /* cached integer type descriptions */
	ctf_id_t dt_type_func;	/* cached CTF identifier for function type */
	ctf_id_t dt_type_fptr;	/* cached CTF identifier for function pointer */
	ctf_id_t dt_type_str;	/* cached CTF identifier for string type */
	ctf_id_t dt_type_dyn;	/* cached CTF identifier for <DYN> type */
	ctf_id_t dt_type_stack;	/* cached CTF identifier for stack type */
	ctf_id_t dt_type_symaddr; /* cached CTF identifier for _symaddr type */
	ctf_id_t dt_type_usymaddr; /* cached CTF ident. for _usymaddr type */
	ctf_id_t dt_type_void;	/* cached CTF identifier for void type */
	dtrace_epid_t dt_nextepid; /* next enabled probe ID to assign */
	size_t dt_maxprobe;	/* max enabled probe ID */
	dtrace_datadesc_t **dt_ddesc; /* probe data descriptions */
	dtrace_probedesc_t **dt_pdesc; /* probe descriptions for enabled prbs */
	size_t dt_maxagg;	/* max aggregation ID */
	dtrace_aggdesc_t **dt_adesc; /* aggregation descriptions */
	int dt_maxformat;	/* max format ID */
	dt_aggregate_t dt_aggregate; /* aggregate */
	struct dt_pebset *dt_pebset; /* perf event buffers set */
	struct dt_pfdict *dt_pfdict; /* dictionary of printf conversions */
	dt_version_t dt_vmax;	/* optional ceiling on program API binding */
	dtrace_attribute_t dt_amin; /* optional floor on program attributes */
	char *dt_cpp_path;	/* pathname of cpp(1) to invoke if needed */
	char **dt_cpp_argv;	/* argument vector for exec'ing cpp(1) */
	int dt_cpp_argc;	/* count of initialized cpp(1) arguments */
	int dt_cpp_args;	/* size of dt_cpp_argv[] array */
	char *dt_ld_path;	/* pathname of ld(1) to invoke if needed */
	dt_list_t dt_lib_path;	/* linked-list forming library search path */
	char *dt_module_path;	/* pathname of kernel module root */
	dt_version_t dt_kernver;/* kernel version, used in the libpath */
	uid_t dt_useruid;	/* lowest non-system uid: set via -xuseruid */
	char *dt_sysslice;	/* the systemd system slice: set via -xsysslice */
	uint_t dt_lazyload;	/* boolean:  set via -xlazyload */
	uint_t dt_droptags;	/* boolean:  set via -xdroptags */
	uint_t dt_active;	/* boolean:  set once tracing is active */
	uint_t dt_stopped;	/* boolean:  set once tracing is stopped */
	processorid_t dt_beganon; /* CPU that executed BEGIN probe (if any) */
	processorid_t dt_endedon; /* CPU that executed END probe (if any) */
	uint_t dt_oflags;	/* dtrace open-time options (see dtrace.h) */
	uint_t dt_cflags;	/* dtrace compile-time options (see dtrace.h) */
	uint_t dt_dflags;	/* dtrace link-time options (see dtrace.h) */
	uint_t dt_prcmode;	/* dtrace process create mode (see dt_proc.h) */
	uint_t dt_linkmode;	/* dtrace symbol linking mode (see below) */
	uint_t dt_linktype;	/* dtrace link output file type (see below) */
	uint_t dt_link_no_mmap;	/* whether linking should avoid mmap() */
	uint_t dt_xlatemode;	/* dtrace translator linking mode (see below) */
	uint_t dt_stdcmode;	/* dtrace stdc compatibility mode (see below) */
	uint_t dt_treedump;	/* dtrace tree debug bitmap (see below) */
	uint_t dt_disasm;	/* dtrace disassembler bitmap (see below) */
	uint64_t dt_options[DTRACEOPT_MAX]; /* dtrace run-time options */
	uint32_t dt_bpfhelper[__BPF_FUNC_MAX_ID]; /* BPF helper mapping */
	int dt_version;		/* library version requested by client */
	int dt_ctferr;		/* error resulting from last CTF failure */
	int dt_errno;		/* error resulting from last failed operation */
	int dt_cdefs_fd;	/* file descriptor for C CTF debugging cache */
	int dt_ddefs_fd;	/* file descriptor for D CTF debugging cache */
	int dt_stdout_fd;	/* file descriptor for saved stdout */
	int dt_poll_fd;		/* file descriptor for event polling */
	int dt_proc_fd;		/* file descriptor for proc eventfd */
	int dt_stmap_fd;	/* file descriptor for the 'state' BPF map */
	int dt_aggmap_fd;	/* file descriptor for the 'aggs' BPF map */
	int dt_genmap_fd;	/* file descriptor for the 'agggen' BPF map */
	int dt_cpumap_fd;	/* file descriptor for the 'cpuinfo' BPF map */
	dtrace_handle_err_f *dt_errhdlr; /* error handler, if any */
	void *dt_errarg;	/* error handler argument */
	dtrace_handle_drop_f *dt_drophdlr; /* drop handler, if any */
	void *dt_droparg;	/* drop handler argument */
	dtrace_handle_proc_f *dt_prochdlr; /* proc handler, if any */
	void *dt_procarg;	/* proc handler argument */
	dtrace_handle_setopt_f *dt_setopthdlr; /* setopt handler, if any */
	void *dt_setoptarg;	/* setopt handler argument */
	dtrace_status_t dt_status[2]; /* status cache */
	dt_percpu_drops_t *dt_drops; /* per-CPU drop counters cache */
	uint64_t dt_specdrops;	/* consumer-side spec drops counter */
	int dt_statusgen;	/* current status generation */
	hrtime_t dt_laststatus;	/* last status */
	hrtime_t dt_lastswitch;	/* last switch of buffer data */
	hrtime_t dt_lastagg;	/* last snapshot of aggregation data */
	dt_list_t dt_spec_bufs_draining; /* List of spec bufs being drained */
	dt_htab_t *dt_spec_bufs;/* spec ID -> list of dt_spec_bufs_head_t */
	char *dt_sprintf_buf;	/* buffer for dtrace_sprintf() */
	int dt_sprintf_buflen;	/* length of dtrace_sprintf() buffer */
	pthread_mutex_t dt_sprintf_lock; /* lock for dtrace_sprintf() buffer */
	const char *dt_filetag;	/* default filetag for dt_set_errmsg() */
	char *dt_buffered_buf;	/* buffer for buffered output */
	size_t dt_buffered_offs; /* current offset into buffered buffer */
	size_t dt_buffered_size; /* size of buffered buffer */
	dtrace_handle_buffered_f *dt_bufhdlr; /* buffered handler, if any */
	void *dt_bufarg;	/* buffered handler argument */
	dt_dof_t dt_dof;	/* DOF generation buffers (see dt_dof.c) */
	struct utsname dt_uts;	/* uname(2) information for system */
	dt_list_t dt_lib_dep;	/* scratch linked-list of lib dependencies */
	dt_list_t dt_lib_dep_sorted;	/* dependency sorted library list */
	dt_global_pcap_t dt_pcap; /* global tshark/pcap state */
	char *dt_freopen_filename; /* filename for freopen() action */
};

/*
 * Values for the user arg of the ECB.
 */
#define	DT_ECB_DEFAULT		0
#define	DT_ECB_ERROR		1

/*
 * Values for the dt_linkmode property, which is used by the assembler when
 * processing external symbol references.  User can set using -xlink=<mode>.
 */
#define	DT_LINK_KERNEL	0	/* kernel syms static, user syms dynamic */
#define	DT_LINK_DYNAMIC	1	/* all symbols dynamic */
#define	DT_LINK_STATIC	2	/* all symbols static */

/*
 * Values for the dt_linktype property, which is used by dtrace_program_link()
 * to determine the type of output file that is desired by the client.
 */
#define	DT_LTYP_ELF	0	/* produce ELF containing DOF */
#define	DT_LTYP_DOF	1	/* produce stand-alone DOF */

/*
 * Values for the dt_xlatemode property, which is used to determine whether
 * references to dynamic translators are permitted.  Set using -xlate=<mode>.
 */
#define	DT_XL_STATIC	0	/* require xlators to be statically defined */
#define	DT_XL_DYNAMIC	1	/* produce references to dynamic translators */

/*
 * Values for the dt_stdcmode property, which is used by the compiler when
 * running cpp to determine the presence and setting of the __STDC__ macro.
 */
#define	DT_STDC_XA	0	/* Strict ISO C. */
#define	DT_STDC_XS	1	/* K&R C: __STDC__ not defined */

/*
 * Values for the DTrace debug assertion property, which turns on assertions
 * which may be expensive while running the testsuite.
 */
#define DT_DEBUG_MUTEXES 0x01

/*
 * Macro to test whether a given pass bit is set in the dt_treedump bit-vector.
 * If the bit for pass 'p' is set, the D compiler displays the parse tree for
 * the program by printing it to stderr at the end of compiler pass 'p'.
 */
#define	DT_TREEDUMP_PASS(dtp, p)	((dtp)->dt_treedump & (1 << ((p) - 1)))

/*
 * Macros for accessing the cached CTF container and type ID for the common
 * types "int", "string", and <DYN>, which we need to use frequently in the D
 * compiler.  The DT_INT_* macro relies upon "int" being at index 0 in the
 * _dtrace_ints_* tables in dt_open.c; the others are also set up there.
 */
#define	DT_INT_CTFP(dtp)	((dtp)->dt_ints[0].did_ctfp)
#define	DT_INT_TYPE(dtp)	((dtp)->dt_ints[0].did_type)

#define	DT_FUNC_CTFP(dtp)	((dtp)->dt_ddefs->dm_ctfp)
#define	DT_FUNC_TYPE(dtp)	((dtp)->dt_type_func)

#define	DT_FPTR_CTFP(dtp)	((dtp)->dt_ddefs->dm_ctfp)
#define	DT_FPTR_TYPE(dtp)	((dtp)->dt_type_fptr)

#define	DT_STR_CTFP(dtp)	((dtp)->dt_ddefs->dm_ctfp)
#define	DT_STR_TYPE(dtp)	((dtp)->dt_type_str)

#define	DT_DYN_CTFP(dtp)	((dtp)->dt_ddefs->dm_ctfp)
#define	DT_DYN_TYPE(dtp)	((dtp)->dt_type_dyn)

#define	DT_STACK_CTFP(dtp)	((dtp)->dt_ddefs->dm_ctfp)
#define	DT_STACK_TYPE(dtp)	((dtp)->dt_type_stack)

#define	DT_SYMADDR_CTFP(dtp)	((dtp)->dt_ddefs->dm_ctfp)
#define	DT_SYMADDR_TYPE(dtp)	((dtp)->dt_type_symaddr)

#define	DT_USYMADDR_CTFP(dtp)	((dtp)->dt_ddefs->dm_ctfp)
#define	DT_USYMADDR_TYPE(dtp)	((dtp)->dt_type_usymaddr)

/*
 * Actions and subroutines are both DT_NODE_FUNC nodes; to avoid confusing
 * an action for a subroutine (or vice versa), we assure that the DT_ACT_*
 * constants and the DIF_SUBR_* constants occupy non-overlapping ranges by
 * starting the DT_ACT_* constants at DIF_SUBR_MAX + 1.
 */
#define	DT_ACT_BASE		(DIF_SUBR_MAX + 1)
#define	DT_ACT(n)		(DT_ACT_BASE + (n))
#define DT_ACT_IDX(n)		((n) - DT_ACT_BASE)

#define	DT_ACT_PRINTF		DT_ACT(0)	/* printf() action */
#define	DT_ACT_TRACE		DT_ACT(1)	/* trace() action */
#define	DT_ACT_TRACEMEM		DT_ACT(2)	/* tracemem() action */
#define	DT_ACT_STACK		DT_ACT(3)	/* stack() action */
#define	DT_ACT_STOP		DT_ACT(4)	/* stop() action */
#define	DT_ACT_BREAKPOINT	DT_ACT(5)	/* breakpoint() action */
#define	DT_ACT_PANIC		DT_ACT(6)	/* panic() action */
#define	DT_ACT_SPECULATE	DT_ACT(7)	/* speculate() action */
#define	DT_ACT_COMMIT		DT_ACT(8)	/* commit() action */
#define	DT_ACT_DISCARD		DT_ACT(9)	/* discard() action */
#define	DT_ACT_CHILL		DT_ACT(10)	/* chill() action */
#define	DT_ACT_EXIT		DT_ACT(11)	/* exit() action */
#define	DT_ACT_USTACK		DT_ACT(12)	/* ustack() action */
#define	DT_ACT_PRINTA		DT_ACT(13)	/* printa() action */
#define	DT_ACT_RAISE		DT_ACT(14)	/* raise() action */
#define	DT_ACT_CLEAR		DT_ACT(15)	/* clear() action */
#define	DT_ACT_NORMALIZE	DT_ACT(16)	/* normalize() action */
#define	DT_ACT_DENORMALIZE	DT_ACT(17)	/* denormalize() action */
#define	DT_ACT_TRUNC		DT_ACT(18)	/* trunc() action */
#define	DT_ACT_SYSTEM		DT_ACT(19)	/* system() action */
#define	DT_ACT_JSTACK		DT_ACT(20)	/* jstack() action */
#define	DT_ACT_FTRUNCATE	DT_ACT(21)	/* ftruncate() action */
#define	DT_ACT_FREOPEN		DT_ACT(22)	/* freopen() action */
#define	DT_ACT_SYM		DT_ACT(23)	/* sym()/func() actions */
#define	DT_ACT_MOD		DT_ACT(24)	/* mod() action */
#define	DT_ACT_USYM		DT_ACT(25)	/* usym()/ufunc() actions */
#define	DT_ACT_UMOD		DT_ACT(26)	/* umod() action */
#define	DT_ACT_UADDR		DT_ACT(27)	/* uaddr() action */
#define	DT_ACT_SETOPT		DT_ACT(28)	/* setopt() action */
#define	DT_ACT_PCAP		DT_ACT(29)	/* pcap() action */

#define DT_ACT_MAX		30

/*
 * Aggregation functions.
 */
#define	DT_AGG_BASE		DTRACEACT_AGGREGATION
#define	DT_AGG(n)		(DT_AGG_BASE + (n))
#define DT_AGG_IDX(n)		((n) - DT_AGG_BASE)
#define DT_AGG_NUM		(DT_AGG_IDX(DT_AGG_HIGHEST))

typedef enum dt_aggfid {
	DT_AGG_AVG = DT_AGG_BASE,
	DT_AGG_COUNT,
	DT_AGG_LLQUANTIZE,
	DT_AGG_LQUANTIZE,
	DT_AGG_MAX,
	DT_AGG_MIN,
	DT_AGG_QUANTIZE,
	DT_AGG_STDDEV,
	DT_AGG_SUM,

	DT_AGG_HIGHEST
} dt_aggfid_t;

/*
 * Sentinel to tell freopen() to restore the saved stdout.  This must not
 * be ever valid for opening for write access via freopen(3C), which of
 * course, "." never is.
 */
#define	DT_FREOPEN_RESTORE	"."

#define	EDT_BASE	1000	/* base value for libdtrace errnos */

enum {
	EDT_VERSION = EDT_BASE,	/* client is requesting unsupported version */
	EDT_VERSINVAL,		/* version string is invalid or overflows */
	EDT_VERSUNDEF,		/* requested API version is not defined */
	EDT_VERSREDUCED,	/* requested API version has been reduced */
	EDT_CTF,		/* libctf called failed (dt_ctferr has more) */
	EDT_COMPILER,		/* error in D program compilation */
	EDT_NOREG,		/* register allocation failure */
	EDT_NOTUPREG,		/* tuple register allocation failure */
	EDT_NOMEM,		/* memory allocation failure */
	EDT_INT2BIG,		/* integer limit exceeded */
	EDT_STR2BIG,		/* string limit exceeded */
	EDT_NOMOD,		/* unknown module name */
	EDT_NOPROV,		/* unknown provider name */
	EDT_NOPROBE,		/* unknown probe name */
	EDT_NOSYM,		/* unknown symbol name */
	EDT_NOSYMADDR,		/* no symbol corresponds to address */
	EDT_NOTYPE,		/* unknown type name */
	EDT_NOVAR,		/* unknown variable name */
	EDT_NOAGG,		/* unknown aggregation name */
	EDT_BADSCOPE,		/* improper use of type name scoping operator */
	EDT_BADSPEC,		/* overspecified probe description */
	EDT_BADSPCV,		/* bad macro variable in probe description */
	EDT_BADID,		/* invalid probe identifier */
	EDT_NOTLOADED,		/* module is not currently loaded */
	EDT_NOCTF,		/* module does not contain any CTF data */
	EDT_DATAMODEL,		/* module and program data models don't match */
	EDT_DIFVERS,		/* library has newer DIF version than driver */
	EDT_BADAGG,		/* unrecognized aggregating action */
	EDT_FIO,		/* error occurred while reading from input stream */
	EDT_BPFINVAL,		/* invalid BPF program */
	EDT_BPFSIZE,		/* BPF program too large */
	EDT_BPFFAULT,		/* BPF program with invalid pointer */
	EDT_BPF,		/* BPF error */
	EDT_BADPROBE,		/* bad probe description */
	EDT_BADPGLOB,		/* bad probe description globbing pattern */
	EDT_NOSCOPE,		/* declaration scope stack underflow */
	EDT_NODECL,		/* declaration stack underflow */
	EDT_DMISMATCH,		/* record list does not match statement */
	EDT_DOFFSET,		/* record data offset error */
	EDT_DALIGN,		/* record data alignment error */
	EDT_DSIZE,		/* record data size error */
	EDT_BADEPID,		/* invalid enabled probe id */
	EDT_BADOPTNAME,		/* invalid dtrace_setopt option name */
	EDT_BADOPTVAL,		/* invalid dtrace_setopt option value */
	EDT_BADOPTCTX,		/* invalid dtrace_setopt option context */
	EDT_CPPFORK,		/* failed to fork preprocessor */
	EDT_CPPEXEC,		/* failed to exec preprocessor */
	EDT_CPPENT,		/* preprocessor not found */
	EDT_CPPERR,		/* unknown preprocessor error */
	EDT_SYMOFLOW,		/* external symbol table overflow */
	EDT_ACTIVE,		/* operation illegal when tracing is active */
	EDT_DESTRUCTIVE,	/* destructive actions not allowed */
	EDT_NOANON,		/* no anonymous tracing state */
	EDT_ISANON,		/* can't claim anon state and enable probes */
	EDT_ENDTOOBIG,		/* END enablings exceed size of prncpl buffer */
	EDT_NOCONV,		/* failed to load type for printf conversion */
	EDT_BADCONV,		/* incomplete printf conversion */
	EDT_BADERROR,		/* invalid library ERROR action */
	EDT_ERRABORT,		/* abort due to error */
	EDT_DROPABORT,		/* abort due to drop */
	EDT_DIRABORT,		/* abort explicitly directed */
	EDT_BADRVAL,		/* invalid return value from callback */
	EDT_BADNORMAL,		/* invalid normalization */
	EDT_BUFTOOSMALL,	/* enabling exceeds size of buffer */
	EDT_BADTRUNC,		/* invalid truncation */
	EDT_BUSY,		/* device busy (active kernel debugger) */
	EDT_ACCESS,		/* insufficient privileges to use DTrace */
	EDT_NOENT,		/* dtrace device not available */
	EDT_BRICKED,		/* abort due to systemic unresponsiveness */
	EDT_HARDWIRE,		/* failed to load hard-wired definitions */
	EDT_ELFVERSION,		/* libelf is out-of-date w.r.t libdtrace */
	EDT_NOBUFFERED,		/* attempt to buffer output without handler */
	EDT_UNSTABLE,		/* description matched unstable set of probes */
	EDT_BADSETOPT,		/* invalid setopt library action */
	EDT_BADSTACKPC,		/* invalid stack program counter size */
	EDT_BADAGGVAR,		/* invalid aggregation variable identifier */
	EDT_OVERSION,		/* client is requesting deprecated version */
	EDT_ENABLING_ERR,	/* failed to enable probe */
	EDT_CORRUPT_KALLSYMS,	/* corrupt /proc/kallsyms */
	EDT_ELFCLASS,		/* unknown ELF class, neither 32- nor 64-bit */
	EDT_OBJIO,		/* cannot read object file or module name mapping */
	EDT_READMAXSTACK,	/* cannot read kernel param perf_event_max_stack */
	EDT_TRACEMEM,		/* missing or corrupt tracemem() record */
	EDT_PCAP		/* missing or corrupt pcap() record */
};

/*
 * Interfaces for parsing and comparing DTrace attribute tuples, which describe
 * stability and architectural binding information.
 */
extern dtrace_attribute_t dt_attr_min(dtrace_attribute_t, dtrace_attribute_t);
extern dtrace_attribute_t dt_attr_max(dtrace_attribute_t, dtrace_attribute_t);
extern char *dt_attr_str(dtrace_attribute_t, char *, size_t);
extern int dt_attr_cmp(dtrace_attribute_t, dtrace_attribute_t);

/*
 * Interfaces for parsing and handling DTrace version strings.  Version binding
 * is a feature of the D compiler that is handled completely independently of
 * the DTrace kernel infrastructure, so the definitions are here in libdtrace.
 * Version strings are compiled into an encoded uint32_t which can be compared
 * using C comparison operators.  Version definitions are found in dt_open.c.
 */
#define	DT_VERSION_STRMAX	16	/* enough for "255.4095.4095\0" */
#define	DT_VERSION_MAJMAX	0xFF	/* maximum major version number */
#define	DT_VERSION_MINMAX	0xFFF	/* maximum minor version number */
#define	DT_VERSION_MICMAX	0xFFF	/* maximum micro version number */

#define	DT_VERSION_NUMBER(M, m, u) \
	((((M) & 0xFF) << 24) | (((m) & 0xFFF) << 12) | ((u) & 0xFFF))

#define	DT_VERSION_MAJOR(v)	(((v) & 0xFF000000) >> 24)
#define	DT_VERSION_MINOR(v)	(((v) & 0x00FFF000) >> 12)
#define	DT_VERSION_MICRO(v)	((v) & 0x00000FFF)

extern char *dt_version_num2str(dt_version_t, char *, size_t);
extern int dt_version_str2num(const char *, dt_version_t *);
extern int dt_version_defined(dt_version_t);

extern int dt_str2kver(const char *, dt_version_t *);

extern uint32_t dt_gen_hval(const char *, uint32_t, size_t);

/*
 * Miscellaneous internal libdtrace interfaces.  The definitions below are for
 * libdtrace routines that do not yet merit their own separate header file.
 */
extern char *dt_cpp_add_arg(dtrace_hdl_t *, const char *);
extern char *dt_cpp_pop_arg(dtrace_hdl_t *);

extern int dt_set_errno(dtrace_hdl_t *, int);
extern void dt_set_errmsg(dtrace_hdl_t *, const char *, const char *,
    const char *, int, const char *, va_list);

extern void dt_get_status(dtrace_hdl_t *dtp);
extern int dt_check_cpudrops(dtrace_hdl_t *dtp, processorid_t cpu,
			     dtrace_dropkind_t what);
extern int dt_ioctl(dtrace_hdl_t *, unsigned long int, void *);
extern int dt_cpu_status(dtrace_hdl_t *, int);
extern long dt_sysconf(dtrace_hdl_t *, int);
extern ssize_t dt_write(dtrace_hdl_t *, int, const void *, size_t);
_dt_printflike_(3,4)
extern int dt_printf(dtrace_hdl_t *, FILE *, const char *, ...);

extern void *dt_zalloc(dtrace_hdl_t *, size_t);
extern void *dt_calloc(dtrace_hdl_t *, size_t, size_t);
extern void *dt_alloc(dtrace_hdl_t *, size_t);
extern void dt_free(dtrace_hdl_t *, void *);
extern void dt_difo_free(dtrace_hdl_t *, dtrace_difo_t *);
extern const char *dt_difo_getstr(const dtrace_difo_t *, ssize_t);

extern void dt_conf_init(dtrace_hdl_t *);

extern int dt_gmatch(const char *, const char *);
extern char *dt_basename(char *);

extern ulong_t dt_popc(ulong_t);
extern ulong_t dt_popcb(const ulong_t *, ulong_t);

extern int dt_buffered_enable(dtrace_hdl_t *);
extern int dt_buffered_flush(dtrace_hdl_t *, dtrace_probedata_t *,
    const dtrace_recdesc_t *, const dtrace_aggdata_t *, uint32_t flags);
extern void dt_buffered_disable(dtrace_hdl_t *);
extern void dt_buffered_destroy(dtrace_hdl_t *);

extern uint64_t dt_stddev(uint64_t *, uint64_t);

extern int dt_options_load(dtrace_hdl_t *);

extern void dt_setcontext(dtrace_hdl_t *, const dtrace_probedesc_t *);
extern void dt_endcontext(dtrace_hdl_t *);

extern void dt_dlib_init(dtrace_hdl_t *dtp);
extern dt_ident_t *dt_dlib_get_sym(dtrace_hdl_t *, const char *);
extern dt_ident_t *dt_dlib_add_func(dtrace_hdl_t *, const char *);
extern dt_ident_t *dt_dlib_get_func(dtrace_hdl_t *, const char *);
extern dt_ident_t *dt_dlib_get_map(dtrace_hdl_t *, const char *);
extern dt_ident_t *dt_dlib_add_var(dtrace_hdl_t *dtp, const char *name,
				   uint_t id);
extern dt_ident_t *dt_dlib_add_probe_var(dtrace_hdl_t *dtp,
					 const struct dt_probe *prp);
extern dt_ident_t *dt_dlib_get_var(dtrace_hdl_t *, const char *);
extern dtrace_difo_t *dt_dlib_get_func_difo(dtrace_hdl_t *, const dt_ident_t *);
extern void dt_dlib_reset(dtrace_hdl_t *dtp, boolean_t);
extern int dt_load_libs(dtrace_hdl_t *dtp);

extern void *dt_compile(dtrace_hdl_t *dtp, int context,
			dtrace_probespec_t pspec, void *arg, uint_t cflags,
			int argc, char *const argv[], FILE *fp, const char *s);
extern dtrace_difo_t *dt_construct(dtrace_hdl_t *dtp, struct dt_probe *prp,
				   uint_t cflags, dt_ident_t *idp);
extern int dt_link(dtrace_hdl_t *dtp, const struct dt_probe *prp,
		   dtrace_difo_t *dp, dt_ident_t *idp);
extern dtrace_difo_t *dt_program_construct(dtrace_hdl_t *dtp,
					   struct dt_probe *prp, uint_t cflags,
					   dt_ident_t *idp);

extern void dt_pragma(dt_node_t *);
extern int dt_reduce(dtrace_hdl_t *, dt_version_t);
extern dtrace_difo_t *dt_as(dt_pcb_t *);
extern dtrace_difo_t *dt_difo_copy(dtrace_hdl_t *dtp, const dtrace_difo_t *odp);

extern int dt_aggregate_go(dtrace_hdl_t *);
extern int dt_aggregate_init(dtrace_hdl_t *);
extern void dt_aggregate_destroy(dtrace_hdl_t *);
extern int dt_aggregate_clear_one(const dtrace_aggdata_t *, void *);

extern int dt_consume_init(dtrace_hdl_t *);
extern void dt_consume_fini(dtrace_hdl_t *);

extern dtrace_datadesc_t *dt_datadesc_hold(dtrace_datadesc_t *ddp);
extern void dt_datadesc_release(dtrace_hdl_t *, dtrace_datadesc_t *);
extern dtrace_datadesc_t *dt_datadesc_create(dtrace_hdl_t *);
extern int dt_datadesc_finalize(dtrace_hdl_t *, dtrace_datadesc_t *);
extern dtrace_epid_t dt_epid_add(dtrace_hdl_t *, dtrace_datadesc_t *,
				 dtrace_id_t);
extern int dt_epid_lookup(dtrace_hdl_t *, dtrace_epid_t, dtrace_datadesc_t **,
			  dtrace_probedesc_t **);
extern void dt_epid_destroy(dtrace_hdl_t *);
typedef void (*dt_cg_gap_f)(dt_pcb_t *, int);
extern uint32_t dt_rec_add(dtrace_hdl_t *, dt_cg_gap_f, dtrace_actkind_t,
			   uint32_t, uint16_t, dt_pfargv_t *, uint64_t);
extern int dt_aggid_add(dtrace_hdl_t *, const dt_ident_t *);
extern int dt_aggid_rec_add(dtrace_hdl_t *dtp, dtrace_aggid_t aggid,
			    uint32_t size, uint16_t alignment);
extern int dt_aggid_lookup(dtrace_hdl_t *, dtrace_aggid_t, dtrace_aggdesc_t **);
extern void dt_aggid_destroy(dtrace_hdl_t *);

extern int dt_print_quantize(dtrace_hdl_t *, FILE *, const void *, size_t,
			     uint64_t);
extern int dt_print_lquantize(dtrace_hdl_t *, FILE *, const void *, size_t,
			      uint64_t, uint64_t);
extern int dt_print_llquantize(dtrace_hdl_t *, FILE *, const void *, size_t,
			       uint64_t, uint64_t);
extern int dt_print_agg(const dtrace_aggdata_t *, void *);

extern int dt_handle(dtrace_hdl_t *, dtrace_probedata_t *);
extern int dt_handle_rawerr(dtrace_hdl_t *, const char *);
extern int dt_handle_liberr(dtrace_hdl_t *,
    const dtrace_probedata_t *, const char *);
extern int dt_handle_cpudrop(dtrace_hdl_t *, processorid_t,
    dtrace_dropkind_t, uint64_t);
extern int dt_handle_status(dtrace_hdl_t *,
    dtrace_status_t *, dtrace_status_t *);
extern int dt_handle_setopt(dtrace_hdl_t *, dtrace_setoptdata_t *);

extern int dt_lib_depend_add(dtrace_hdl_t *, dt_list_t *, const char *);
extern dt_lib_depend_t *dt_lib_depend_lookup(dt_list_t *, const char *);

extern dt_pcb_t *yypcb;		/* pointer to current parser control block */
extern char yyintprefix;	/* int token prefix for macros (+/-) */
extern char yyintsuffix[4];	/* int token suffix ([uUlL]*) */
extern int yyintdecimal;	/* int token is decimal (1) or octal/hex (0) */
extern char *yytext;		/* lex input buffer */
extern int yylineno;		/* lex line number */
extern int yydebug;		/* lex debugging */
extern dt_node_t *yypragma;	/* lex token list for control lines */

extern const dtrace_attribute_t _dtrace_maxattr; /* maximum attributes */
extern const dtrace_attribute_t _dtrace_defattr; /* default attributes */
extern const dtrace_attribute_t _dtrace_symattr; /* symbol ref attributes */
extern const dtrace_attribute_t _dtrace_typattr; /* type ref attributes */
extern const dtrace_attribute_t _dtrace_prvattr; /* provider attributes */
extern const dtrace_pattr_t _dtrace_prvdesc;	 /* provider attribute bundle */

extern const dt_version_t _dtrace_versions[];	 /* array of valid versions */
extern const char *const _dtrace_version;	 /* current version string */

extern int _dtrace_strbuckets;		/* number of hash buckets for strings */
extern uint_t _dtrace_stkindent;	/* default indent for stack/ustack */
extern uint_t _dtrace_pidbuckets;	/* number of hash buckets for pids */
extern uint_t _dtrace_pidlrulim;	/* number of proc handles to cache */
extern size_t _dtrace_bufsize;		/* default dt_buf_create() size */
extern int _dtrace_argmax;		/* default maximum probe arguments */
extern int _dtrace_debug_assert;	/* turn on expensive assertions */
extern int _dtrace_stackframes;		/* default number of stack frames */
extern int _dtrace_ustackframes;	/* default number of user stack frames */

extern const char *_dtrace_moddir;	/* default kernel module directory */

#pragma GCC poison bcopy bzero bcmp

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_IMPL_H */
