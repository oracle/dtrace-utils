/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_CG_H
#define	_DT_CG_H

#include <dt_as.h>
#include <dt_pcb.h>
#include <dt_state.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void dt_cg(dt_pcb_t *, dt_node_t *);
extern void dt_cg_xsetx(dt_irlist_t *, dt_ident_t *, uint_t, int, uint64_t);
extern dt_irnode_t *dt_cg_node_alloc(uint_t, struct bpf_insn);
extern void dt_cg_tramp_prologue_act(dt_pcb_t *pcb, dt_activity_t act);
extern void dt_cg_tramp_prologue(dt_pcb_t *pcb);
extern void dt_cg_tramp_call_clauses(dt_pcb_t *pcb, dt_activity_t act);
extern void dt_cg_tramp_return(dt_pcb_t *pcb);
extern void dt_cg_tramp_epilogue(dt_pcb_t *pcb);
extern void dt_cg_tramp_epilogue_advance(dt_pcb_t *pcb, dt_activity_t act);
extern void dt_cg_tramp_error(dt_pcb_t *pcb);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_CG_H */
