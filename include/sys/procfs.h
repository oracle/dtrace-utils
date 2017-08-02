/*
 * Oracle Linux DTrace.
 * Copyright © 2006, 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
	char	*pr_mapaddrname; /* address text from /proc/<pid>/maps */
	size_t	pr_size;	/* size of mapping in bytes */
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
