/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_AS_H
#define	_DT_AS_H

#include <sys/types.h>
#include <sys/dtrace.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_irnode {
	uint_t di_label;		/* label number or DT_LBL_NONE */
	dif_instr_t di_instr;		/* instruction opcode */
	void *di_extern;		/* opcode-specific external reference */
	struct dt_irnode *di_next;	/* next instruction */
} dt_irnode_t;

#define	DT_LBL_NONE	0		/* no label on this instruction */

typedef struct dt_irlist {
	dt_irnode_t *dl_list;		/* pointer to first node in list */
	dt_irnode_t *dl_last;		/* pointer to last node in list */
	uint_t dl_len;			/* number of valid instructions */
	uint_t dl_label;		/* next label number to assign */
} dt_irlist_t;

extern void dt_irlist_create(dt_irlist_t *);
extern void dt_irlist_destroy(dt_irlist_t *);
extern void dt_irlist_append(dt_irlist_t *, dt_irnode_t *);
extern uint_t dt_irlist_label(dt_irlist_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_AS_H */
