/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * DTrace Parsing Control Block
 *
 * A DTrace Parsing Control Block (PCB) contains all of the state that is used
 * by a single pass of the D compiler, other than the global variables used by
 * lex and yacc.  The routines in this file are used to set up and tear down
 * PCBs, which are kept on a stack pointed to by the libdtrace global 'yypcb'.
 * The main engine of the compiler, dt_compile(), is located in dt_cc.c and is
 * responsible for calling these routines to begin and end a compilation pass.
 *
 * Sun's lex/yacc are not MT-safe or re-entrant, but we permit limited nested
 * use of dt_compile() once the entire parse tree has been constructed but has
 * not yet executed the "cooking" pass (see dt_cc.c for more information).  The
 * PCB design also makes it easier to debug (since all global state is kept in
 * one place) and could permit us to make the D compiler MT-safe or re-entrant
 * in the future by adding locks to libdtrace or switching to Flex and Bison.
 */

#include <stdlib.h>
#include <assert.h>

#include <dt_impl.h>
#include <dt_program.h>
#include <dt_provider.h>
#include <dt_probe.h>
#include <dt_pcb.h>

/*
 * Initialize the specified PCB by zeroing it and filling in a few default
 * members, and then pushing it on to the top of the PCB stack and setting
 * yypcb to point to it.  Increment the current handle's generation count.
 */
void
dt_pcb_push(dtrace_hdl_t *dtp, dt_pcb_t *pcb)
{
	/*
	 * Since lex/yacc are not re-entrant and we don't implement state save,
	 * assert that if another PCB is active, it is from the same handle and
	 * has completed execution of yyparse().  If the first assertion fires,
	 * the caller is calling libdtrace without proper MT locking.  If the
	 * second assertion fires, dt_compile() is being called recursively
	 * from an illegal location in libdtrace, or a dt_pcb_pop() is missing.
	 */
	if (yypcb != NULL) {
		assert(yypcb->pcb_hdl == dtp);
		assert(yypcb->pcb_yystate == YYS_DONE);
	}

	memset(pcb, 0, sizeof(dt_pcb_t));

	dt_scope_create(&pcb->pcb_dstack);
	dt_idstack_push(&pcb->pcb_globals, dtp->dt_globals);
	dt_irlist_create(&pcb->pcb_ir);
	pcb->pcb_exitlbl = dt_irlist_label(&pcb->pcb_ir);

	pcb->pcb_hdl = dtp;
	pcb->pcb_prev = dtp->dt_pcb;

	dtp->dt_pcb = pcb;
	dtp->dt_gen++;

	yyinit(pcb);
}

static int
dt_pcb_pop_ident(dt_idhash_t *dhp, dt_ident_t *idp, void *arg)
{
	dtrace_hdl_t *dtp = arg;

	if (idp->di_gen == dtp->dt_gen)
		dt_idhash_delete(dhp, idp);

	return 0;
}

static int
dt_pcb_destroy_probe(dtrace_hdl_t *dtp, dt_probe_t *prp, void *dummy)
{
	dt_probe_destroy(prp);
	return 0;
}

/*
 * Pop the topmost PCB from the PCB stack and destroy any data structures that
 * are associated with it.  If 'err' is non-zero, destroy any intermediate
 * state that is left behind as part of a compilation that has failed.
 */
void
dt_pcb_pop(dtrace_hdl_t *dtp, int err)
{
	dt_pcb_t *pcb = yypcb;
	uint_t i;

	assert(pcb != NULL);
	assert(pcb == dtp->dt_pcb);

	while (pcb->pcb_dstack.ds_next != NULL)
		dt_scope_pop();

	dt_scope_destroy(&pcb->pcb_dstack);
	dt_irlist_destroy(&pcb->pcb_ir);

	dt_node_link_free(&pcb->pcb_list);
	dt_node_link_free(&pcb->pcb_hold);

	if (err != 0) {
		dt_xlator_t *dxp, *nxp;
		dt_provider_t *pvp, *nvp;

		if (pcb->pcb_prog != NULL)
			dt_program_destroy(dtp, pcb->pcb_prog);
		if (pcb->pcb_stmt != NULL)
			dtrace_stmt_destroy(dtp, pcb->pcb_stmt);
		if (pcb->pcb_ecbdesc != NULL)
			dt_ecbdesc_release(dtp, pcb->pcb_ecbdesc);

		for (dxp = dt_list_next(&dtp->dt_xlators); dxp; dxp = nxp) {
			nxp = dt_list_next(dxp);
			if (dxp->dx_gen == dtp->dt_gen)
				dt_xlator_destroy(dtp, dxp);
		}

		for (pvp = dt_list_next(&dtp->dt_provlist); pvp; pvp = nvp) {
			nvp = dt_list_next(pvp);
			if (pvp->pv_gen == dtp->dt_gen) {
				dtrace_probedesc_t	pdp;

				/* Destroy all probes for this provider. */
				pdp.id = DTRACE_IDNONE;
				pdp.prv = pvp->desc.dtvd_name;
				pdp.mod = "";
				pdp.fun = "";
				pdp.prb = "";
				dt_probe_iter(dtp, &pdp, dt_pcb_destroy_probe,
					      NULL, NULL);

				dt_provider_destroy(dtp, pvp);
			}
		}

		dt_idhash_iter(dtp->dt_aggs, dt_pcb_pop_ident, dtp);
		dt_idhash_update(dtp->dt_aggs);

		dt_idhash_iter(dtp->dt_globals, dt_pcb_pop_ident, dtp);
		dt_idhash_update(dtp->dt_globals);

		dt_idhash_iter(dtp->dt_tls, dt_pcb_pop_ident, dtp);
		dt_idhash_update(dtp->dt_tls);

		ctf_discard(dtp->dt_cdefs->dm_ctfp);
		ctf_discard(dtp->dt_ddefs->dm_ctfp);
	}

	if (dtp->dt_maxlvaralloc < pcb->pcb_locals->dh_nextoff)
		dtp->dt_maxlvaralloc = pcb->pcb_locals->dh_nextoff;

	if (pcb->pcb_pragmas != NULL)
		dt_idhash_destroy(pcb->pcb_pragmas);
	if (pcb->pcb_locals != NULL)
		dt_idhash_destroy(pcb->pcb_locals);
	if (pcb->pcb_idents != NULL)
		dt_idhash_destroy(pcb->pcb_idents);
	if (pcb->pcb_regs != NULL)
		dt_regset_destroy(pcb->pcb_regs);

	for (i = 0; i < pcb->pcb_asxreflen; i++)
		dt_free(dtp, pcb->pcb_asxrefs[i]);

	dt_free(dtp, pcb->pcb_asxrefs);
	dt_difo_free(dtp, pcb->pcb_difo);

	free(pcb->pcb_filetag);
	free(pcb->pcb_sflagv);

	dtp->dt_pcb = pcb->pcb_prev;
	memset(pcb, 0, sizeof(dt_pcb_t));
	yyinit(dtp->dt_pcb);
}
