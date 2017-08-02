/*
 * Oracle Linux DTrace.
 * Copyright © 2009, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

#ifndef _SYS_MNTTAB_H
#define	_SYS_MNTTAB_H

#include <sys/types.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MNTTAB	"/etc/mnttab"
#define	MNT_LINE_MAX	1024

#define	MNT_TOOLONG	1	/* entry exceeds MNT_LINE_MAX */
#define	MNT_TOOMANY	2	/* too many fields in line */
#define	MNT_TOOFEW	3	/* too few fields in line */

#define	mntnull(mp)\
	((mp)->mnt_special = (mp)->mnt_mountp = \
	    (mp)->mnt_fstype = (mp)->mnt_mntopts = \
	    (mp)->mnt_time = NULL)

#define	putmntent(fd, mp)	(-1)

/*
 * The fields in struct extmnttab should match those in struct mnttab until new
 * fields are encountered. This allows hasmntopt(), getmntent_common() and
 * mntioctl() to cast one type to the other safely.
 *
 * The fields in struct mnttab, struct extmnttab and struct mntentbuf must all
 * match those in the corresponding 32-bit versions defined in mntvnops.c.
 */
struct mnttab {
	char	*mnt_special;
	char	*mnt_mountp;
	char	*mnt_fstype;
	char	*mnt_mntopts;
	char	*mnt_time;
};

struct extmnttab {
	char	*mnt_special;
	char	*mnt_mountp;
	char	*mnt_fstype;
	char	*mnt_mntopts;
	char	*mnt_time;
	uint_t	mnt_major;
	uint_t	mnt_minor;
};

struct mntentbuf {
	struct	extmnttab *mbuf_emp;
	size_t 	mbuf_bufsize;
	char	*mbuf_buf;
};

#if !defined(_KERNEL)
#ifdef __STDC__
extern void	resetmnttab(FILE *);
extern int	getmntent(FILE *, struct mnttab *);
extern int	getextmntent(FILE *, struct extmnttab *, size_t);
extern int	getmntany(FILE *, struct mnttab *, struct mnttab *);
extern char	*hasmntopt(struct mnttab *, char *);
extern char	*mntopt(char **);
#else
extern void	resetmnttab();
extern int	getmntent();
extern int	getextmntent();
extern int	getmntany();
extern char	*hasmntopt();
extern char	*mntopt();
#endif
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MNTTAB_H */
