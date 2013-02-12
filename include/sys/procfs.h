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
#include <sys/dtrace_types.h>
#include <sys/procfs_isa.h>
#include <dt_list.h>
#include <link.h>

/*
 * The prmap_file points to all mappings corresponding to a single file, sorted
 * by address.  prmap_files are hashed by name (including the terminating \0,
 * so anonymous maps are all hashed together).
 */
struct prmap;
typedef struct prmap_file {
	struct prmap_file *prf_next;	/* next in hash chain */
	char	*prf_mapname;		/* name in /proc/<pid>/maps */
	struct prmap **prf_mappings;	/* sorted by address */
	size_t prf_num_mappings;	/* number of mappings */
	struct prmap *prf_text_map;	/* primary text mapping, if known */
	struct prmap *prf_data_map;	/* primary data mapping, if known */
} prmap_file_t;

/*
 * A single mapping.
 */
typedef struct prmap {
	uintptr_t pr_vaddr;	/* virtual address of mapping */
	size_t	pr_size;	/* size of mapping in bytes */
	offset_t pr_offset;	/* offset into mapped object, if any */
	int	pr_mflags;	/* protection and attribute flags (see below) */
	dev_t	pr_dev;		/* device number */
	ino_t	pr_inum;	/* inode number */
	prmap_file_t *pr_file;	/* backpointer to corresponding file mapping */
} prmap_t;


/* Protection and attribute flags */
#define	MA_READ		0x04	/* readable by the traced process */
#define	MA_WRITE	0x02	/* writable by the traced process */
#define	MA_EXEC		0x01	/* executable by the traced process */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_H */
