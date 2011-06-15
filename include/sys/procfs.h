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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_PROCFS_SOLARIS_H
#define	_SYS_PROCFS_SOLARIS_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/procfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This definition is temporary.  Structured proc is the preferred API,
 * and the older ioctl-based interface will be removed in a future version
 * of Solaris.  Until then, by default, including <sys/procfs.h> will
 * provide the older ioctl-based /proc definitions.  To get the structured
 * /proc definitions, either include <procfs.h> or define _STRUCTURED_PROC
 * to be 1 before including <sys/procfs.h>.
 */
#ifndef	_STRUCTURED_PROC
#define	_STRUCTURED_PROC	0
#endif

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
//#include <sys/pset.h>
#include <sys/procfs_isa.h>
#include <sys/ucontext.h>
#include <sys/processor.h>


/*
 * System call interfaces for /proc.
 */

/*
 * Reasons for stopping (pr_why).
 */
#define	PR_REQUESTED	1
#define	PR_SIGNALLED	2
#define	PR_JOBCONTROL	5
#define	PR_FAULTED	6
#define	PR_SUSPENDED	7
#define	PR_CHECKPOINT	8

#define	PRNODEV	(dev_t)(-1)	/* non-existent device */

/*
 * Memory-map interface.  /proc/<pid>/map /proc/<pid>/rmap
 */
#define	PRMAPSZ	64
typedef struct prmap {
	uintptr_t pr_vaddr;	/* virtual address of mapping */
	size_t	pr_size;	/* size of mapping in bytes */
	char	pr_mapname[PRMAPSZ];	/* name in /proc/<pid>/object */
	offset_t pr_offset;	/* offset into mapped object, if any */
	int	pr_mflags;	/* protection and attribute flags (see below) */
	int	pr_pagesize;	/* pagesize (bytes) for this mapping */
	int	pr_shmid;	/* SysV shmid, -1 if not SysV shared memory */
	int	pr_filler[1];	/* filler for future expansion */
} prmap_t;


/* Protection and attribute flags */
#define	MA_READ		0x04	/* readable by the traced process */
#define	MA_WRITE	0x02	/* writable by the traced process */
#define	MA_EXEC		0x01	/* executable by the traced process */
#define	MA_SHARED	0x08	/* changes are shared by mapped object */
#define	MA_ANON		0x40	/* anonymous memory (e.g. /dev/zero) */
#define	MA_ISM		0x80	/* intimate shared mem (shared MMU resources) */
#define	MA_NORESERVE	0x100	/* mapped with MAP_NORESERVE */
#define	MA_SHM		0x200	/* System V shared memory */
#define	MA_RESERVED1	0x400	/* reserved for future use */

/*
 * These are obsolete and unreliable.
 * They are included here only for historical compatibility.
 */
#define	MA_BREAK	0x10	/* grown by brk(2) */
#define	MA_STACK	0x20	/* grown automatically on stack faults */

/*
 * _ILP32 PCREAD/PCWRITE I/O interface.
 */
typedef struct priovec32 {
	caddr32_t pio_base;	/* buffer in controlling process */
	size32_t pio_len;	/* size of read/write request */
	off32_t	pio_offset;	/* virtual address in target process */
} priovec32_t;

/*
 * _ILP32 Resource usage.  /proc/<pid>/usage /proc/<pid>/lwp/<lwpid>/lwpusage
 */
typedef struct prusage32 {
	id32_t		pr_lwpid;	/* lwp id.  0: process or defunct */
	int32_t		pr_count;	/* number of contributing lwps */
	timestruc32_t	pr_tstamp;	/* current time stamp */
	timestruc32_t	pr_create;	/* process/lwp creation time stamp */
	timestruc32_t	pr_term;	/* process/lwp termination time stamp */
	timestruc32_t	pr_rtime;	/* total lwp real (elapsed) time */
	timestruc32_t	pr_utime;	/* user level cpu time */
	timestruc32_t	pr_stime;	/* system call cpu time */
	timestruc32_t	pr_ttime;	/* other system trap cpu time */
	timestruc32_t	pr_tftime;	/* text page fault sleep time */
	timestruc32_t	pr_dftime;	/* data page fault sleep time */
	timestruc32_t	pr_kftime;	/* kernel page fault sleep time */
	timestruc32_t	pr_ltime;	/* user lock wait sleep time */
	timestruc32_t	pr_slptime;	/* all other sleep time */
	timestruc32_t	pr_wtime;	/* wait-cpu (latency) time */
	timestruc32_t	pr_stoptime;	/* stopped time */
	timestruc32_t	filltime[6];	/* filler for future expansion */
	uint32_t	pr_minf;	/* minor page faults */
	uint32_t	pr_majf;	/* major page faults */
	uint32_t	pr_nswap;	/* swaps */
	uint32_t	pr_inblk;	/* input blocks */
	uint32_t	pr_oublk;	/* output blocks */
	uint32_t	pr_msnd;	/* messages sent */
	uint32_t	pr_mrcv;	/* messages received */
	uint32_t	pr_sigs;	/* signals received */
	uint32_t	pr_vctx;	/* voluntary context switches */
	uint32_t	pr_ictx;	/* involuntary context switches */
	uint32_t	pr_sysc;	/* system calls */
	uint32_t	pr_ioch;	/* chars read and written */
	uint32_t	filler[10];	/* filler for future expansion */
} prusage32_t;

/*
 * _ILP32 Page data file.  /proc/<pid>/pagedata
 */

/* _ILP32 page data file header */
typedef struct prpageheader32 {
	timestruc32_t	pr_tstamp;	/* real time stamp */
	int32_t		pr_nmap;	/* number of address space mappings */
	int32_t		pr_npage;	/* total number of pages */
} prpageheader32_t;

/* _ILP32 page data mapping header */
typedef struct prasmap32 {
	caddr32_t pr_vaddr;	/* virtual address of mapping */
	size32_t pr_npage;	/* number of pages in mapping */
	char	pr_mapname[64];	/* name in /proc/<pid>/object */
	offset_t pr_offset;	/* offset into mapped object, if any */
	int	pr_mflags;	/* protection and attribute flags */
	int	pr_pagesize;	/* pagesize (bytes) for this mapping */
	int	pr_shmid;	/* SysV shmid, -1 if not SysV shared memory */
	int	pr_filler[1];	/* filler for future expansion */
} prasmap32_t;

/*
 * _ILP32 Header for /proc/<pid>/lstatus /proc/<pid>/lpsinfo /proc/<pid>/lusage
 */
typedef struct prheader32 {
	int32_t	pr_nent;	/* number of entries */
	int32_t	pr_entsize;	/* size of each entry, in bytes */
} prheader32_t;


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_H */
