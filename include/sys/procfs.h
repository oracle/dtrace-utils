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
 * Copyright 2006, 2012, 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_PROCFS_SOLARIS_H
#define	_SYS_PROCFS_SOLARIS_H

#include <sys/procfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/procfs_isa.h>

/*
 * System call interfaces for /proc.
 */

/*
 * Memory-map interface.  /proc/<pid>/map /proc/<pid>/rmap
 */
typedef struct prmap {
	uintptr_t pr_vaddr;	/* virtual address of mapping */
	size_t	pr_size;	/* size of mapping in bytes */
	char	*pr_mapname;	/* name in /proc/<pid>/object */
	offset_t pr_offset;	/* offset into mapped object, if any */
	int	pr_mflags;	/* protection and attribute flags (see below) */
	int	pr_pagesize;	/* pagesize (bytes) for this mapping */
	ino_t	pr_inum;	/* inode number */
} prmap_t;


/* Protection and attribute flags */
#define	MA_READ		0x04	/* readable by the traced process */
#define	MA_WRITE	0x02	/* writable by the traced process */
#define	MA_EXEC		0x01	/* executable by the traced process */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_H */
