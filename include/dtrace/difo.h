/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2021, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Note: The contents of this file are private to the implementation of the
 * DTrace subsystem and are subject to change at any time without notice.
 */

#ifndef _DTRACE_DIFO_H
#define _DTRACE_DIFO_H

#include <dtrace/universal.h>
#include <dtrace/dif.h>
#include <dtrace/dof_defines.h>
#include <dtrace/metadesc.h>
#include <linux/bpf.h>

/*
 * A DIFO is used to store the compiled DIF for a D expression, its return
 * type, and its string and variable tables.  The string table is a single
 * buffer of character data into which sets instructions and variable
 * references can reference strings using a byte offset.  The variable table
 * is an array of dtrace_difv_t structures that describe the name and type of
 * each variable and the id used in the DIF code.  This structure is described
 * above in the DIF section of this header file.  The DIFO is used at both
 * user-level (in the library) and in the kernel, but the structure is never
 * passed between the two: the DOF structures form the only interface.  As a
 * result, the definition can change depending on the presence of _KERNEL.
 */

typedef struct dtrace_difo {
	struct bpf_insn *dtdo_buf;		/* instruction buffer */
	char *dtdo_strtab;			/* string table (optional) */
	dtrace_difv_t *dtdo_vartab;		/* variable table (optional) */
	uint_t dtdo_len;			/* length of instruction buffer */
	uint_t dtdo_strlen;			/* length of string table */
	uint_t dtdo_varlen;			/* length of variable table */
	uint_t dtdo_reclen;			/* length of trace record */
	uint_t dtdo_refcnt;			/* owner reference count */
	uint_t dtdo_flags;			/* flags (destructive, ...) */
	struct dof_relodesc *dtdo_breltab;	/* BPF relocations */
	struct dof_relodesc *dtdo_kreltab;	/* kernel relocations */
	struct dof_relodesc *dtdo_ureltab;	/* user relocations */
	struct dt_node **dtdo_xlmtab;		/* translator references */
	uint_t dtdo_brelen;			/* length of brelo table */
	uint_t dtdo_krelen;			/* length of krelo table */
	uint_t dtdo_urelen;			/* length of urelo table */
	uint_t dtdo_xlmlen;			/* length of translator table */
	dtrace_datadesc_t *dtdo_ddesc;		/* metadata record description */
} dtrace_difo_t;

#endif /* _DTRACE_DIFO_H */
