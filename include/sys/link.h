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
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 * Copyright (c) 1989, 2010, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef _SYS_LINK_H
#define	_SYS_LINK_H

#ifndef	_ASM
#include <sys/types.h>
#include <gelf.h>
#endif
#include <link.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Communication structures for the runtime linker.
 */

#if 0
/*
 * The following data structure provides a self-identifying union consisting
 * of a tag from a known list and a value.
 */
#ifndef	_ASM
typedef struct {
	Elf32_Sword d_tag;		/* how to interpret value */
	union {
		Elf32_Word	d_val;
		Elf32_Addr	d_ptr;
		Elf32_Off	d_off;
	} d_un;
} Elf32_Dyn;

#if defined(_LP64) || defined(_LONGLONG_TYPE)
typedef struct {
	Elf64_Xword d_tag;		/* how to interpret value */
	union {
		Elf64_Xword	d_val;
		Elf64_Addr	d_ptr;
	} d_un;
} Elf64_Dyn;
#endif	/* defined(_LP64) || defined(_LONGLONG_TYPE) */
#endif	/* _ASM */

#endif

/*
 * Tag values
 */
#define	DT_NULL		0	/* last entry in list */
#define	DT_NEEDED	1	/* a needed object */
#define	DT_PLTRELSZ	2	/* size of relocations for the PLT */
#define	DT_PLTGOT	3	/* addresses used by procedure linkage table */
#define	DT_HASH		4	/* hash table */
#define	DT_STRTAB	5	/* string table */
#define	DT_SYMTAB	6	/* symbol table */
#define	DT_RELA		7	/* addr of relocation entries */
#define	DT_RELASZ	8	/* size of relocation table */
#define	DT_RELAENT	9	/* base size of relocation entry */
#define	DT_STRSZ	10	/* size of string table */
#define	DT_SYMENT	11	/* size of symbol table entry */
#define	DT_INIT		12	/* _init addr */
#define	DT_FINI		13	/* _fini addr */
#define	DT_SONAME	14	/* name of this shared object */
#define	DT_RPATH	15	/* run-time search path */
#define	DT_SYMBOLIC	16	/* shared object linked -Bsymbolic */
#define	DT_REL		17	/* addr of relocation entries */
#define	DT_RELSZ	18	/* size of relocation table */
#define	DT_RELENT	19	/* base size of relocation entry */
#define	DT_PLTREL	20	/* relocation type for PLT entry */
#define	DT_DEBUG	21	/* pointer to r_debug structure */
#define	DT_TEXTREL	22	/* text relocations remain for this object */
#define	DT_JMPREL	23	/* pointer to the PLT relocation entries */
#define	DT_BIND_NOW	24	/* perform all relocations at load of object */
#define	DT_INIT_ARRAY	25	/* pointer to .init_array */
#define	DT_FINI_ARRAY	26	/* pointer to .fini_array */
#define	DT_INIT_ARRAYSZ	27	/* size of .init_array */
#define	DT_FINI_ARRAYSZ	28	/* size of .fini_array */
#define	DT_RUNPATH	29	/* run-time search path */
#define	DT_FLAGS	30	/* state flags - see DF_* */

/*
 * DT_* encoding rules: The value of each dynamic tag determines the
 * interpretation of the d_un union. This convention provides for simpler
 * interpretation of dynamic tags by external tools. A tag whose value
 * is an even number indicates a dynamic section entry that uses d_ptr.
 * A tag whose value is an odd number indicates a dynamic section entry
 * that uses d_val, or that uses neither d_ptr nor d_val.
 *
 * There are exceptions to the above rule:
 *	- Tags with values that are less than DT_ENCODING.
 *	- Tags with values that fall between DT_LOOS and DT_SUNW_ENCODING
 *	- Tags with values that fall between DT_HIOS and DT_LOPROC
 *
 * Third party tools must handle these exception ranges explicitly
 * on an item by item basis.
 */
#define	DT_ENCODING		32	/* positive tag DT_* encoding rules */
					/*	start after this */
#define	DT_PREINIT_ARRAY	32	/* pointer to .preinit_array */
#define	DT_PREINIT_ARRAYSZ	33	/* size of .preinit_array */

#define	DT_MAXPOSTAGS		34	/* number of positive tags */

/*
 * DT_* encoding rules do not apply between DT_LOOS and DT_SUNW_ENCODING
 */
#define	DT_LOOS			0x6000000d	/* OS specific range */
#define	DT_SUNW_AUXILIARY	0x6000000d	/* symbol auxiliary name */
#define	DT_SUNW_RTLDINF		0x6000000e	/* ld.so.1 info (private) */
#define	DT_SUNW_FILTER		0x6000000f	/* symbol filter name */
#define	DT_SUNW_CAP		0x60000010	/* hardware/software */
						/*	capabilities */
#define	DT_SUNW_SYMTAB		0x60000011	/* symtab with local fcn */
						/*	symbols immediately */
						/*	preceding DT_SYMTAB */
#define	DT_SUNW_SYMSZ		0x60000012	/* Size of SUNW_SYMTAB table */

/*
 * DT_* encoding rules apply between DT_SUNW_ENCODING and DT_HIOS
 */
#define	DT_SUNW_ENCODING	0x60000013	/* DT_* encoding rules resume */
						/*	after this */
#define	DT_SUNW_SORTENT		0x60000013	/* sizeof [SYM|TLS]SORT entry */
#define	DT_SUNW_SYMSORT		0x60000014	/* sym indices sorted by addr */
#define	DT_SUNW_SYMSORTSZ	0x60000015	/* size of SUNW_SYMSORT */
#define	DT_SUNW_TLSSORT		0x60000016	/* tls sym ndx sort by offset */
#define	DT_SUNW_TLSSORTSZ	0x60000017	/* size of SUNW_TLSSORT */
#define	DT_SUNW_CAPINFO		0x60000018	/* capabilities symbols */
#define	DT_SUNW_STRPAD		0x60000019	/* # of unused bytes at the */
						/*	end of dynstr */
#define	DT_SUNW_CAPCHAIN	0x6000001a	/* capabilities chain info */
#define	DT_SUNW_LDMACH		0x6000001b	/* EM_ machine code of linker */
						/*	that produced object */
#define	DT_SUNW_CAPCHAINENT	0x6000001d	/* capabilities chain entry */
#define	DT_SUNW_CAPCHAINSZ	0x6000001f	/* capabilities chain size */

/*
 * DT_* encoding rules do not apply between DT_HIOS and DT_LOPROC
 */
#define	DT_HIOS			0x6ffff000

/*
 * The following values have been deprecated and remain here to allow
 * compatibility with older binaries.
 */
#define	DT_DEPRECATED_SPARC_REGISTER	0x7000001

/*
 * DT_* entries which fall between DT_VALRNGHI & DT_VALRNGLO use the
 * Dyn.d_un.d_val field of the Elf*_Dyn structure.
 */
#define	DT_VALRNGLO	0x6ffffd00

#define	DT_GNU_PRELINKED 0x6ffffdf5	/* prelinking timestamp (unused) */
#define	DT_GNU_CONFLICTSZ 0x6ffffdf6	/* size of conflict section (unused) */
#define	DT_GNU_LIBLISTSZ 0x6ffffdf7	/* size of library list (unused) */
#define	DT_CHECKSUM	0x6ffffdf8	/* elf checksum */
#define	DT_PLTPADSZ	0x6ffffdf9	/* pltpadding size */
#define	DT_MOVEENT	0x6ffffdfa	/* move table entry size */
#define	DT_MOVESZ	0x6ffffdfb	/* move table size */
#define	DT_FEATURE_1	0x6ffffdfc	/* feature holder (unused) */
#define	DT_POSFLAG_1	0x6ffffdfd	/* flags for DT_* entries, effecting */
					/*	the following DT_* entry. */
					/*	See DF_P1_* definitions */
#define	DT_SYMINSZ	0x6ffffdfe	/* syminfo table size (in bytes) */
#define	DT_SYMINENT	0x6ffffdff	/* syminfo entry size (in bytes) */
#define	DT_VALRNGHI	0x6ffffdff

/*
 * DT_* entries which fall between DT_ADDRRNGHI & DT_ADDRRNGLO use the
 * Dyn.d_un.d_ptr field of the Elf*_Dyn structure.
 *
 * If any adjustment is made to the ELF object after it has been
 * built, these entries will need to be adjusted.
 */
#define	DT_ADDRRNGLO	0x6ffffe00

#define	DT_GNU_HASH	0x6ffffef5	/* GNU-style hash table (unused) */
#define	DT_TLSDESC_PLT	0x6ffffef6	/* GNU (unused) */
#define	DT_TLSDESC_GOT	0x6ffffef7	/* GNU (unused) */
#define	DT_GNU_CONFLICT	0x6ffffef8	/* start of conflict section (unused) */
#define	DT_GNU_LIBLIST	0x6ffffef9	/* Library list (unused) */

#define	DT_CONFIG	0x6ffffefa	/* configuration information */
#define	DT_DEPAUDIT	0x6ffffefb	/* dependency auditing */
#define	DT_AUDIT	0x6ffffefc	/* object auditing */
#define	DT_PLTPAD	0x6ffffefd	/* pltpadding (sparcv9) */
#define	DT_MOVETAB	0x6ffffefe	/* move table */
#define	DT_SYMINFO	0x6ffffeff	/* syminfo table */
#define	DT_ADDRRNGHI	0x6ffffeff

/*
 * The following DT_* entries should have been assigned within one of the
 * DT_* ranges, but existed before such ranges had been established.
 */
#define	DT_VERSYM	0x6ffffff0	/* version symbol table - unused by */
					/*	Solaris (see libld/update.c) */

#define	DT_RELACOUNT	0x6ffffff9	/* number of RELATIVE relocations */
#define	DT_RELCOUNT	0x6ffffffa	/* number of RELATIVE relocations */
#define	DT_FLAGS_1	0x6ffffffb	/* state flags - see DF_1_* defs */
#define	DT_VERDEF	0x6ffffffc	/* version definition table and */
#define	DT_VERDEFNUM	0x6ffffffd	/*	associated no. of entries */
#define	DT_VERNEED	0x6ffffffe	/* version needed table and */
#define	DT_VERNEEDNUM	0x6fffffff	/* 	associated no. of entries */

/*
 * DT_* entries between DT_HIPROC and DT_LOPROC are reserved for processor
 * specific semantics.
 *
 * DT_* encoding rules apply to all tag values larger than DT_LOPROC.
 */
#define	DT_LOPROC	0x70000000	/* processor specific range */
#define	DT_AUXILIARY	0x7ffffffd	/* shared library auxiliary name */
#define	DT_USED		0x7ffffffe	/* ignored - same as needed */
#define	DT_FILTER	0x7fffffff	/* shared library filter name */
#define	DT_HIPROC	0x7fffffff


/*
 * Values for DT_FLAGS
 */
#define	DF_ORIGIN	0x00000001	/* ORIGIN processing required */
#define	DF_SYMBOLIC	0x00000002	/* symbolic bindings in effect */
#define	DF_TEXTREL	0x00000004	/* text relocations remain */
#define	DF_BIND_NOW	0x00000008	/* process all relocations */
#define	DF_STATIC_TLS	0x00000010	/* obj. contains static TLS refs */

/*
 * Values set to DT_FEATURE_1 tag's d_val (unused obsolete tag)
 */
#define	DTF_1_PARINIT	0x00000001	/* partially initialization feature */
#define	DTF_1_CONFEXP	0x00000002	/* configuration file expected */

#if 0

/*
 * Version structures.  There are three types of version structure:
 *
 *  o	A definition of the versions within the image itself.
 *	Each version definition is assigned a unique index (starting from
 *	VER_NDX_BGNDEF)	which is used to cross-reference symbols associated to
 *	the version.  Each version can have one or more dependencies on other
 *	version definitions within the image.  The version name, and any
 *	dependency names, are specified in the version definition auxiliary
 *	array.  Version definition entries require a version symbol index table.
 *
 *  o	A version requirement on a needed dependency.  Each needed entry
 *	specifies the shared object dependency (as specified in DT_NEEDED).
 *	One or more versions required from this dependency are specified in the
 *	version needed auxiliary array.
 *
 *  o	A version symbol index table.  Each symbol indexes into this array
 *	to determine its version index.  Index values of VER_NDX_BGNDEF or
 *	greater indicate the version definition to which a symbol is associated.
 *	(the size of a symbol index entry is recorded in the sh_info field).
 */
#ifndef	_ASM

typedef struct {			/* Version Definition Structure. */
	Elf32_Half	vd_version;	/* this structures version revision */
	Elf32_Half	vd_flags;	/* version information */
	Elf32_Half	vd_ndx;		/* version index */
	Elf32_Half	vd_cnt;		/* no. of associated aux entries */
	Elf32_Word	vd_hash;	/* version name hash value */
	Elf32_Word	vd_aux;		/* no. of bytes from start of this */
					/*	verdef to verdaux array */
	Elf32_Word	vd_next;	/* no. of bytes from start of this */
} Elf32_Verdef;				/*	verdef to next verdef entry */

typedef struct {			/* Verdef Auxiliary Structure. */
	Elf32_Word	vda_name;	/* first element defines the version */
					/*	name. Additional entries */
					/*	define dependency names. */
	Elf32_Word	vda_next;	/* no. of bytes from start of this */
} Elf32_Verdaux;			/*	verdaux to next verdaux entry */


typedef	struct {			/* Version Requirement Structure. */
	Elf32_Half	vn_version;	/* this structures version revision */
	Elf32_Half	vn_cnt;		/* no. of associated aux entries */
	Elf32_Word	vn_file;	/* name of needed dependency (file) */
	Elf32_Word	vn_aux;		/* no. of bytes from start of this */
					/*	verneed to vernaux array */
	Elf32_Word	vn_next;	/* no. of bytes from start of this */
} Elf32_Verneed;			/*	verneed to next verneed entry */

typedef struct {			/* Verneed Auxiliary Structure. */
	Elf32_Word	vna_hash;	/* version name hash value */
	Elf32_Half	vna_flags;	/* version information */
	Elf32_Half	vna_other;
	Elf32_Word	vna_name;	/* version name */
	Elf32_Word	vna_next;	/* no. of bytes from start of this */
} Elf32_Vernaux;			/*	vernaux to next vernaux entry */

typedef	Elf32_Half 	Elf32_Versym;	/* Version symbol index array */

typedef struct {
	Elf32_Half	si_boundto;	/* direct bindings - symbol bound to */
	Elf32_Half	si_flags;	/* per symbol flags */
} Elf32_Syminfo;


#if defined(_LP64) || defined(_LONGLONG_TYPE)
typedef struct {
	Elf64_Half	vd_version;	/* this structures version revision */
	Elf64_Half	vd_flags;	/* version information */
	Elf64_Half	vd_ndx;		/* version index */
	Elf64_Half	vd_cnt;		/* no. of associated aux entries */
	Elf64_Word	vd_hash;	/* version name hash value */
	Elf64_Word	vd_aux;		/* no. of bytes from start of this */
					/*	verdef to verdaux array */
	Elf64_Word	vd_next;	/* no. of bytes from start of this */
} Elf64_Verdef;				/*	verdef to next verdef entry */

typedef struct {
	Elf64_Word	vda_name;	/* first element defines the version */
					/*	name. Additional entries */
					/*	define dependency names. */
	Elf64_Word	vda_next;	/* no. of bytes from start of this */
} Elf64_Verdaux;			/*	verdaux to next verdaux entry */

typedef struct {
	Elf64_Half	vn_version;	/* this structures version revision */
	Elf64_Half	vn_cnt;		/* no. of associated aux entries */
	Elf64_Word	vn_file;	/* name of needed dependency (file) */
	Elf64_Word	vn_aux;		/* no. of bytes from start of this */
					/*	verneed to vernaux array */
	Elf64_Word	vn_next;	/* no. of bytes from start of this */
} Elf64_Verneed;			/*	verneed to next verneed entry */

typedef struct {
	Elf64_Word	vna_hash;	/* version name hash value */
	Elf64_Half	vna_flags;	/* version information */
	Elf64_Half	vna_other;
	Elf64_Word	vna_name;	/* version name */
	Elf64_Word	vna_next;	/* no. of bytes from start of this */
} Elf64_Vernaux;			/*	vernaux to next vernaux entry */

typedef	Elf64_Half	Elf64_Versym;

typedef struct {
	Elf64_Half	si_boundto;	/* direct bindings - symbol bound to */
	Elf64_Half	si_flags;	/* per symbol flags */
} Elf64_Syminfo;
#endif	/* defined(_LP64) || defined(_LONGLONG_TYPE) */

#endif	/* _ASM */

#endif

/*
 * Versym symbol index values.  Values greater than VER_NDX_GLOBAL
 * and less then VER_NDX_LORESERVE associate symbols with user
 * specified version descriptors.
 */
#define	VER_NDX_LOCAL		0	/* symbol is local */
#define	VER_NDX_GLOBAL		1	/* symbol is global and assigned to */
					/*	the base version */
#define	VER_NDX_LORESERVE	0xff00	/* beginning of RESERVED entries */
#define	VER_NDX_ELIMINATE	0xff01	/* symbol is to be eliminated */

/*
 * Verdef (vd_flags) and Vernaux (vna_flags) flags values.
 */
#define	VER_FLG_BASE		0x1	/* version definition of file itself */
					/*	(Verdef only) */
#define	VER_FLG_WEAK		0x2	/* weak version identifier */
#define	VER_FLG_INFO		0x4	/* version is recorded in object for */
					/*	informational purposes */
					/*	(Versym reference) only. No */
					/*	runtime verification is */
					/*	required. (Vernaux only) */

/*
 * Verdef version values.
 */
#define	VER_DEF_NONE		0	/* Ver_def version */
#define	VER_DEF_CURRENT		1
#define	VER_DEF_NUM		2

/*
 * Verneed version values.
 */
#define	VER_NEED_NONE		0	/* Ver_need version */
#define	VER_NEED_CURRENT	1
#define	VER_NEED_NUM		2


/*
 * Syminfo flag values
 */
#define	SYMINFO_FLG_DIRECT	0x0001	/* symbol ref has direct association */
					/*	to object containing defn. */
#define	SYMINFO_FLG_FILTER	0x0002	/* symbol ref is associated to a */
					/* 	standard filter */
#if 0
#define	SYMINFO_FLG_PASSTHRU	SYMINFO_FLG_FILTER /* unused obsolete name */
#endif
#define	SYMINFO_FLG_COPY	0x0004	/* symbol is a copy-reloc */
#define	SYMINFO_FLG_LAZYLOAD	0x0008	/* object containing defn. should be */
					/*	lazily-loaded */
#define	SYMINFO_FLG_DIRECTBIND	0x0010	/* ref should be bound directly to */
					/*	object containing defn. */
#define	SYMINFO_FLG_NOEXTDIRECT	0x0020	/* don't let an external reference */
					/*	directly bind to this symbol */
#define	SYMINFO_FLG_AUXILIARY	0x0040	/* symbol ref is associated to a */
					/* 	auxiliary filter */
#define	SYMINFO_FLG_INTERPOSE	0x0080	/* symbol defines an interposer */
#define	SYMINFO_FLG_CAP		0x0100	/* symbol is capabilities specific */
#define	SYMINFO_FLG_DEFERRED	0x0200	/* symbol should not be included in */
					/*	BIND_NOW relocations */

/*
 * Syminfo.si_boundto values.
 */
#define	SYMINFO_BT_SELF		0xffff	/* symbol bound to self */
#define	SYMINFO_BT_PARENT	0xfffe	/* symbol bound to parent */
#define	SYMINFO_BT_NONE		0xfffd	/* no special symbol binding */
#define	SYMINFO_BT_EXTERN	0xfffc	/* symbol defined as external */
#define	SYMINFO_BT_LOWRESERVE	0xff00	/* beginning of reserved entries */

/*
 * Syminfo version values.
 */
#define	SYMINFO_NONE		0	/* Syminfo version */
#define	SYMINFO_CURRENT		1
#define	SYMINFO_NUM		2


/*
 * Public structure defined and maintained within the runtime linker
 */
#ifndef	_ASM

typedef struct link_map	Link_map;
#ifdef _SYSCALL32
typedef struct link_map32 Link_map32;

struct link_map32 {
	Elf32_Word	l_addr;
	Elf32_Addr	l_name;
	Elf32_Addr	l_ld;
	Elf32_Addr	l_next;
	Elf32_Addr	l_prev;
	Elf32_Addr	l_refname;
};
#endif
typedef enum {
	RD_FL_NONE = 0,		/* no flags */
	RD_FL_ODBG = (1<<0),	/* old style debugger present */
	RD_FL_DBG = (1<<1)	/* debugging enabled */
} rd_flags_e;



/*
 * Debugging events enabled inside of the runtime linker.  To
 * access these events see the librtld_db interface.
 */
typedef enum {
	RD_NONE = 0,		/* no event */
	RD_PREINIT,		/* the Initial rendezvous before .init */
	RD_POSTINIT,		/* the Second rendezvous after .init */
	RD_DLACTIVITY		/* a dlopen or dlclose has happened */
} rd_event_e;

#define	R_DEBUG_VERSION	2		/* current r_debug version */
#endif	/* _ASM */

/*
 * Attribute/value structures used to bootstrap ELF-based dynamic linker.
 */
#ifndef	_ASM
typedef struct {
	Elf32_Sword eb_tag;		/* what this one is */
	union {				/* possible values */
		Elf32_Word eb_val;
		Elf32_Addr eb_ptr;
		Elf32_Off  eb_off;
	} eb_un;
} Elf32_Boot;

#if defined(_LP64) || defined(_LONGLONG_TYPE)
typedef struct {
	Elf64_Xword eb_tag;		/* what this one is */
	union {				/* possible values */
		Elf64_Xword eb_val;
		Elf64_Addr eb_ptr;
		Elf64_Off eb_off;
	} eb_un;
} Elf64_Boot;
#endif	/* defined(_LP64) || defined(_LONGLONG_TYPE) */
#endif	/* _ASM */

/*
 * Attributes
 */
#define	EB_NULL		0		/* (void) last entry */
#define	EB_DYNAMIC	1		/* (*) dynamic structure of subject */
#define	EB_LDSO_BASE	2		/* (caddr_t) base address of ld.so */
#define	EB_ARGV		3		/* (caddr_t) argument vector */
#define	EB_ENVP		4		/* (char **) environment strings */
#define	EB_AUXV		5		/* (auxv_t *) auxiliary vector */
#define	EB_DEVZERO	6		/* (int) fd for /dev/zero */
#define	EB_PAGESIZE	7		/* (int) page size */
#define	EB_MAX		8		/* number of "EBs" */
#define	EB_MAX_SIZE32	64		/* size in bytes, _ILP32 */
#define	EB_MAX_SIZE64	128		/* size in bytes, _LP64 */


#ifndef	_ASM

#ifdef __STDC__

/*
 * Concurrency communication structure for libc callbacks.
 */
extern void	_ld_libc(void *);
#else /* __STDC__ */
extern void	_ld_libc();
#endif /* __STDC__ */

#pragma unknown_control_flow(_ld_libc)
#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LINK_H */
