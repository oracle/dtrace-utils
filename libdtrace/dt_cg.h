/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_CG_H
#define	_DT_CG_H

#include <dt_as.h>
#include <dt_pcb.h>
#include <dt_probe.h>
#include <dt_state.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void dt_cg(dt_pcb_t *, dt_node_t *);
extern void dt_cg_xsetx(dt_irlist_t *, dt_ident_t *, uint_t, int, uint64_t);
extern dt_irnode_t *dt_cg_node_alloc(uint_t, struct bpf_insn);
extern void dt_cg_tramp_prologue_act(dt_pcb_t *pcb, dt_activity_t act);
extern void dt_cg_tramp_prologue_cpu(dt_pcb_t *pcb);
extern void dt_cg_tramp_prologue(dt_pcb_t *pcb);
extern void dt_cg_tramp_clear_regs(dt_pcb_t *pcb);
extern void dt_cg_tramp_copy_regs(dt_pcb_t *pcb);
extern void dt_cg_tramp_copy_args_from_regs(dt_pcb_t *pcb, int called);
extern void dt_cg_tramp_copy_pc_from_regs(dt_pcb_t *pcb);
extern void dt_cg_tramp_copy_rval_from_regs(dt_pcb_t *pcb);
extern void dt_cg_tramp_decl_var(dt_pcb_t *pcb, dt_ident_t *idp);
extern void dt_cg_tramp_get_var(dt_pcb_t *pcb, const char *name, int isstore,
				int reg);
extern void dt_cg_tramp_del_var(dt_pcb_t *pcb, const char *name);
extern void dt_cg_tramp_save_args(dt_pcb_t *pcb);
extern void dt_cg_tramp_restore_args(dt_pcb_t *pcb);
extern void dt_cg_tramp_map_args(dt_pcb_t *pcb, dt_argdesc_t *args, size_t nargs);
extern void dt_cg_tramp_call_clauses(dt_pcb_t *pcb, const dt_probe_t *prp,
				     dt_activity_t act);
extern void dt_cg_tramp_return(dt_pcb_t *pcb);
extern void dt_cg_tramp_epilogue(dt_pcb_t *pcb);
extern void dt_cg_tramp_epilogue_advance(dt_pcb_t *pcb, dt_activity_t act);
extern void dt_cg_tramp_error(dt_pcb_t *pcb);
extern int dt_cg_ctf_offsetof(const char *structname, const char *membername,
			      size_t *sizep, int relaxed);
extern uint_t dt_cg_ldsize(dt_node_t *dnp, ctf_file_t *ctfp, ctf_id_t type,
			 ssize_t *ret_size);
extern uint_t bpf_ldst_size(ssize_t size, int store);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_CG_H */
