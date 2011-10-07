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
 * Copyright 2006 Oracle, Inc.  All rights reserved.
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
#include <sys/procfs_isa.h>


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

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_H */
