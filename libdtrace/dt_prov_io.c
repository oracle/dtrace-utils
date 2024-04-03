/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The 'io' SDT provider for DTrace-specific probes.
 *
 * These io::: probes all provide three probe arguments:
 *	(bufinfo_t *, devinfo_t *, fileinfo_t *)
 * where the first two are populated by translators based on a 'struct bio *'
 * argument provided by the trampoline code.  The third probe argument is
 * always 0 on Linux.
 *
 * Most underlying probes provide the bio pointer as an argument.
 *
 * The nfs_* and xfs_* underlying probes do not provide a bio pointer.  For
 * them, we construct a "fake" struct bio in the -io-bio TLS variable based on
 * the implementation specific arguments.
 *
 * For the submit_bio_wait-based probe, we store the bio pointer in the
 * -io-bio-ptr TLS variable at function entry, and retrieve it at function
 * return.
 */
#include <assert.h>
#include <errno.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_provider_sdt.h"
#include "dt_probe.h"

/* Defined in include/linux/blk_types.h */
#define REQ_OP_READ	0
#define REQ_OP_WRITE	1
/* Defined in fs/xfs/xfs_buf.h */
#define XBF_WRITE	(1 << 1) /* buffer intended for writing to device */

static const char	prvname[] = "io";
static const char	modname[] = "vmlinux";  // FIXME:  Really?  Or blank?

/*
 * If the set of functions in the fbt probes changes,
 * update the list in test/unittest/io/tst.fbt_probes.r.
 */
static probe_dep_t	probes[] = {
	{ "wait-start",
	  DTRACE_PROBESPEC_NAME,	"fbt::submit_bio_wait:entry" },
	{ "wait-start",
	  DTRACE_PROBESPEC_NAME,	"rawtp:xfs::xfs_buf_iowait" },
	{ "wait-done",
	  DTRACE_PROBESPEC_FUNC,	"fbt::submit_bio_wait" },
	{ "wait-done",
	  DTRACE_PROBESPEC_NAME,	"rawtp:xfs::xfs_buf_iowait_done" },
	{ "done",
	  DTRACE_PROBESPEC_NAME,	"rawtp:block::block_bio_complete" },
	{ "done",
	  DTRACE_PROBESPEC_NAME,	"rawtp:block::block_rq_complete" },
	{ "done",
	  DTRACE_PROBESPEC_NAME,	"rawtp:nfs::nfs_readpage_done" },
	{ "done",
	  DTRACE_PROBESPEC_NAME,	"rawtp:nfs::nfs_writeback_done" },
	{ "start",
	  DTRACE_PROBESPEC_NAME,	"rawtp:block::block_bio_queue" },
	{ "start",
	  DTRACE_PROBESPEC_NAME,	"rawtp:nfs::nfs_initiate_read",
	  DT_VERSION_NUMBER(5, 6, 0), },
	{ "start",
	  DTRACE_PROBESPEC_NAME,	"fbt:nfs:nfs_initiate_read:entry",
	  0, DT_VERSION_NUMBER(5, 5, 19) },
	{ "start",
	  DTRACE_PROBESPEC_NAME,	"rawtp:nfs::nfs_initiate_write",
	  DT_VERSION_NUMBER(5, 6, 0), },
	{ "start",
	  DTRACE_PROBESPEC_NAME,	"fbt:nfs:nfs_initiate_write:entry",
	  0, DT_VERSION_NUMBER(5, 5, 19) },
	{ NULL, }
};

/*
 * All four probes have three probe args.  The first two will be extracted
 * by a translator from the (struct bio *) we supply.  The (struct file *)
 * we supply will be 0 in all cases.
 */
static probe_arg_t probe_args[] = {
	{ "start", 0, { 0, 0, "struct bio *", "bufinfo_t *" } },
	{ "start", 1, { 0, 0, "struct bio *", "devinfo_t *" } },
	{ "start", 2, { 1, 0, "struct file *", "fileinfo_t *", } },
	{ "done", 0, { 0, 0, "struct bio *", "bufinfo_t *" } },
	{ "done", 1, { 0, 0, "struct bio *", "devinfo_t *" } },
	{ "done", 2, { 1, 0, "struct file *", "fileinfo_t *", } },
	{ "wait-start", 0, { 0, 0, "struct bio *", "bufinfo_t *" } },
	{ "wait-start", 1, { 0, 0, "struct bio *", "devinfo_t *" } },
	{ "wait-start", 2, { 1, 0, "struct file *", "fileinfo_t *", } },
	{ "wait-done", 0, { 0, 0, "struct bio *", "bufinfo_t *" } },
	{ "wait-done", 1, { 0, 0, "struct bio *", "devinfo_t *" } },
	{ "wait-done", 2, { 1, 0, "struct file *", "fileinfo_t *", } },
	{ NULL, }
};

/* List of provider-specific variables. */
static dt_ident_t v_bio = { "-io-bio", DT_IDENT_SCALAR,
			    DT_IDFLG_LOCAL | DT_IDFLG_WRITE, 0, DT_ATTR_STABCMN,
			    DT_VERS_2_0, &dt_idops_type, "struct bio" };
static dt_ident_t v_biop = { "-io-bio-ptr", DT_IDENT_SCALAR,
			     DT_IDFLG_TLS | DT_IDFLG_WRITE, 0, DT_ATTR_STABCMN,
			     DT_VERS_2_0, &dt_idops_type, "struct bio *" };

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
};

/*
 * Provide all the "io" SDT probes.
 */
static int populate(dtrace_hdl_t *dtp)
{
	return dt_sdt_populate(dtp, prvname, modname, &dt_io, &pattr,
			       probe_args, probes);
}

/*
 * Generate BPF instructions to dereference the pointer in %r3 (after applying
 * an optional addend) and read a value of the given 'width'.  The result is
 * stored in register 'reg' (where BPF_REG_0 <= reg <= BPF_REG_5).
 *
 * Registers %r0-%r5 will be clobbered.  Register 'reg' holds the value.
 */
static void deref_r3(dt_irlist_t *dlp, uint_t exitlbl, int addend, int width,
		     int reg)
{
	assert(reg >= BPF_REG_0 && reg <= BPF_REG_5);

	/* Use slot 0 as temporary storage. */
	emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_TRAMP_SP_SLOT(0)));

	/* Specify the width of the scalar. */
	emit(dlp, BPF_MOV_IMM(BPF_REG_2, width));

	/* The source address is already in %r3, but add addend, if any. */
	if (addend)
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, addend));

	/* Perform the copy and check for success. */
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
	emit(dlp, BPF_BRANCH_IMM(BPF_JSLT, BPF_REG_0, 0, exitlbl));

	/* Load the result into the specified register. */
	width = bpf_ldst_size(width, 0);
	emit(dlp, BPF_LOAD(width, reg, BPF_REG_FP, DT_TRAMP_SP_SLOT(0)));
}

/*
 * For NFS events, we have to construct a fake struct bio, which we have to
 * populate from the nfs_pgio_header argument the underlying probe provides.
 */
static void io_nfs_args(dt_pcb_t *pcb, dt_irlist_t *dlp, uint_t exitlbl,
			const char *prb, const char *uprb)
{
	int	off;
	size_t	siz;

	/*
	 * Determine the various sizes and offsets we want.
	 *
	 *     // Access these fields relative to &bio.
	 *     struct bio bio = {
	 *         .bi_opf = ...,
	 *         .bi_iter.bi_size = ...,      // struct bvec_iter bi_iter
	 *         .bi_iter.bi_sector = ...,
	 *         .bi_bdev = 0,		// -or- .bi_disk = 0
	 *     };
	 *
	 *     // Access these fields relative to hdr.
	 *     struct nfs_pgio_header *hdr;
	 *     ... = hdr->args.count;           // struct nfs_pgio_args args
	 *     ... = hdr->res.count;            // struct nfs_pgio_res  res
	 */

	/*
	 * Declare the -io-bio variable and store its address in %r6.
	 */
	dt_cg_tramp_decl_var(pcb, &v_bio);
	dt_cg_tramp_get_var(pcb, "this->-io-bio", 1, BPF_REG_6);

	/* Fill in bi_opf */
	off = dt_cg_ctf_offsetof("struct bio", "bi_opf", &siz, 0);
	siz = bpf_ldst_size(siz, 1);
	if (strstr(uprb, "read"))
		emit(dlp, BPF_STORE_IMM(siz, BPF_REG_6, off, REQ_OP_READ));
	else
		emit(dlp, BPF_STORE_IMM(siz, BPF_REG_6, off, REQ_OP_WRITE));

	/*
	 * bio.bi_iter.bi_size = hdr->foo.count;
	 *
	 * hdr is:
	 *   - arg0 for start
	 *   - arg1 for done
	 */
	if (strcmp(prb, "start") == 0) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
		off = dt_cg_ctf_offsetof("struct nfs_pgio_header", "args", NULL, 0)
		    + dt_cg_ctf_offsetof("struct nfs_pgio_args", "count", &siz, 0);
	} else {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(1)));
		off = dt_cg_ctf_offsetof("struct nfs_pgio_header", "res", NULL, 0)
		    + dt_cg_ctf_offsetof("struct nfs_pgio_res", "count", &siz, 0);
	}
	deref_r3(dlp, exitlbl, off, siz, BPF_REG_0);
	off = dt_cg_ctf_offsetof("struct bio", "bi_iter", NULL, 0)
	    + dt_cg_ctf_offsetof("struct bvec_iter", "bi_size", &siz, 0);
	siz = bpf_ldst_size(siz, 1);
	emit(dlp, BPF_STORE(siz, BPF_REG_6, off, BPF_REG_0));

	/*
	 * bio.bi_iter.bi_sector = hdr->inode;
	 */
	/* get hdr */
	if (strcmp(prb, "start") == 0)
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
	else
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(1)));

	off = dt_cg_ctf_offsetof("struct nfs_pgio_header", "inode", &siz, 0);
	deref_r3(dlp, exitlbl, off, siz, BPF_REG_3);

	off = dt_cg_ctf_offsetof("struct nfs_inode", "fileid", &siz, 0)
	    - dt_cg_ctf_offsetof("struct nfs_inode", "vfs_inode", NULL, 0);

	deref_r3(dlp, exitlbl, off, siz, BPF_REG_0);

	off = dt_cg_ctf_offsetof("struct bio", "bi_iter", NULL, 0)
	    + dt_cg_ctf_offsetof("struct bvec_iter", "bi_sector", &siz, 0);
	siz = bpf_ldst_size(siz, 1);
	emit(dlp, BPF_STORE(siz, BPF_REG_6, off, BPF_REG_0));

	/*
	 * bio.bi_bdev = 0;
	 */
	off = dt_cg_ctf_offsetof("struct bio", "bi_bdev", &siz, 1);
	if (off == -1)
		off = dt_cg_ctf_offsetof("struct bio", "bi_disk", &siz, 0);
	siz = bpf_ldst_size(siz, 1);
	emit(dlp, BPF_STORE_IMM(siz, BPF_REG_6, off, 0));

	/* Store a pointer to the fake bio in arg0. */
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_6));
}

/*
 * For XFS events, we have to construct a fake struct bio, which we have to
 * populate from the xfs_buf argument the underlying probe provides.
 */
static void io_xfs_args(dt_pcb_t *pcb, dt_irlist_t *dlp, uint_t exitlbl)
{
	int	off;
	size_t	siz;

	/*
	 * Determine the various sizes and offsets we want.
	 *
	 *     // Access these fields relative to &bio.
	 *     struct bio bio = {
	 *         .bi_opf = ...,
	 *         .bi_iter.bi_size = ...,      // struct bvec_iter bi_iter
	 *         .bi_iter.bi_sector = ...,
	 *         .bi_bdev = ...,		// -or- .bi_disk = ...
	 *                                      // and  .bi_partno = ...
	 *     };
	 *
	 *     // Access these fields relative to bp.
	 *     struct xfs_buf *bp;
	 *     ... = (bp)->b_flags;
	 *     ... = xfs_buf_daddr(bp);
	 *     ... = (bp)->b_length;
	 *     ... = (bp)->b_target->bt_bdev;   // struct xfs_buftarg *b_target;
	 */

	/*
	 * Declare the -io-bio variable and store its address in %r6.
	 */
	dt_cg_tramp_decl_var(pcb, &v_bio);
	dt_cg_tramp_get_var(pcb, "this->-io-bio", 1, BPF_REG_6);

	/* bio.bi_opf = (bp->b_flags & XBF_WRITE) ? REQ_OP_WRITE : REQ_OP_READ; */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
	off = dt_cg_ctf_offsetof("struct xfs_buf", "b_flags", &siz, 0);
	deref_r3(dlp, exitlbl, off, siz, BPF_REG_0);
	emit(dlp, BPF_ALU64_IMM(BPF_AND, BPF_REG_0, XBF_WRITE));
	{
		uint_t Lzero = dt_irlist_label(dlp);
		uint_t Ldone = dt_irlist_label(dlp);

		off = dt_cg_ctf_offsetof("struct bio", "bi_opf", &siz, 0);
		siz = bpf_ldst_size(siz, 1);

		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, Lzero));
		emit(dlp,  BPF_STORE_IMM(siz, BPF_REG_6, off, REQ_OP_WRITE));
		emit(dlp,  BPF_JUMP(Ldone));
		emitl(dlp, Lzero,
			   BPF_NOP());
		emit(dlp,  BPF_STORE_IMM(siz, BPF_REG_6, off, REQ_OP_READ));
		emitl(dlp, Ldone,
			   BPF_NOP());
	}

	/*
	 * bio.bi_iter.bi_size = bp->b_length;
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
	off = dt_cg_ctf_offsetof("struct xfs_buf", "b_length", &siz, 0);
	deref_r3(dlp, exitlbl, off, siz, BPF_REG_0);
	off = dt_cg_ctf_offsetof("struct bio", "bi_iter", NULL, 0)
	    + dt_cg_ctf_offsetof("struct bvec_iter", "bi_size", &siz, 0);
	siz = bpf_ldst_size(siz, 1);
	emit(dlp, BPF_STORE(siz, BPF_REG_6, off, BPF_REG_0));

	/*
	 * bio.bi_iter.bi_sector = xfs_buf_daddr(bp);
	 *
	 * In fs/xfs/xfs_buf.h, we have
	 *
	 *     xfs_daddr_t xfs_buf_daddr(struct xfs_buf *bp)
	 *     {
	 *         return bp->b_maps[0].bm_bn;
	 *     }
	 *
	 * So that gives
	 *     bio.bi_iter.bi_sector = bp->b_maps->bm_bn;
	 *
	 * include/linux/blk_types.h
	 *     struct bio {
	 *         [...]
	 *         struct bvec_iter        bi_iter;
	 *         [...]
	 *     }
	 * include/linux/bvec.h
	 *     struct bvec_iter {
	 *         sector_t                bi_sector;
	 *         [...]
	 *     };
	 * fs/xfs/xfs_buf.h
	 *     struct xfs_buf_map {
	 *         xfs_daddr_t             bm_bn;
	 *         [...]
	 *     };
	 *     struct xfs_buf {
	 *         [...]
	 *         struct xfs_buf_map      *b_maps;
	 *         [...]
	 *     }
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
	off = dt_cg_ctf_offsetof("struct xfs_buf", "b_maps", &siz, 0);
	deref_r3(dlp, exitlbl, off, siz, BPF_REG_3);
	off = dt_cg_ctf_offsetof("struct xfs_buf_map", "bm_bn", &siz, 0);
	deref_r3(dlp, exitlbl, off, siz, BPF_REG_0);
	off = dt_cg_ctf_offsetof("struct bio", "bi_iter", NULL, 0)
	    + dt_cg_ctf_offsetof("struct bvec_iter", "bi_sector", &siz, 0);
	siz = bpf_ldst_size(siz, 1);
	emit(dlp, BPF_STORE(siz, BPF_REG_6, off, BPF_REG_0));

	/*
	 * bio.bi_bdev = (bp)->b_target->bt_bdev
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
	off = dt_cg_ctf_offsetof("struct xfs_buf", "b_target", &siz, 0);
	assert(siz == sizeof(void *));
	deref_r3(dlp, exitlbl, off, 8, BPF_REG_3);
	off = dt_cg_ctf_offsetof("struct xfs_buftarg", "bt_bdev", &siz, 0);
	deref_r3(dlp, exitlbl, off, siz, BPF_REG_3);
	off = dt_cg_ctf_offsetof("struct bio", "bi_bdev", &siz, 1);
	if (off == -1)
		off = dt_cg_ctf_offsetof("struct bio", "bi_disk", &siz, 0);
	siz = bpf_ldst_size(siz, 1);
	emit(dlp, BPF_STORE(siz, BPF_REG_6, off, BPF_REG_0));

	/* Populate bi_partno if it exists. */
	off = dt_cg_ctf_offsetof("struct bio", "bi_partno", &siz, 1);
	if (off >= 0) {
		int	poff;
		size_t	psiz;

		poff = dt_cg_ctf_offsetof("struct block_device", "bd_partno", &psiz, 0);
		siz = bpf_ldst_size(siz, 1);
		deref_r3(dlp, exitlbl, poff, psiz, BPF_REG_0);
		emit(dlp, BPF_STORE(siz, BPF_REG_6, off, BPF_REG_0));
	}

	/* Store a pointer to the fake bio in arg0. */
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_6));
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_io(void *data)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns the value that it gets
 * back from that function.
 */
static int trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_probe_t	*prp = pcb->pcb_probe;
	dt_probe_t	*uprp = pcb->pcb_parent_probe;

	/*
	 * The nfs_* and xfs_* probes do not pass a bio argument, and therefore
	 * we need to synthesize one.
	 */
	if (strcmp(uprp->desc->mod, "nfs") == 0) {
		/*
		 * If the underlying probe is an FBT probe, we pass function
		 * name.  Otherwise, pass probe name.
		 */
		io_nfs_args(pcb, dlp, exitlbl, prp->desc->prb,
			    strcmp(uprp->desc->prb, "entry") == 0
				? uprp->desc->fun : uprp->desc->prb);
		goto done;
	} else if (strcmp(uprp->desc->mod, "xfs") == 0) {
		io_xfs_args(pcb, dlp, exitlbl);
		goto done;
	}

	/* Handle the start and done probes (non-XFS, non-NFS). */
	if (strcmp(prp->desc->prb, "start") == 0) {
		/*
		 * Older kernels pass 2 arguments to block_bio_queue, and bio
		 * is in arg1.  Newer kernels have bio in arg0 already.
		 */
		if (uprp->nargc == 2) {
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
		}

		goto done;
	} else if (strcmp(prp->desc->prb, "done") == 0) {
		if (strcmp(uprp->desc->prb, "block_bio_complete") == 0) {
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
		} else {
			size_t	size;
			size_t	off;

			off = dt_cg_ctf_offsetof("struct request", "bio", &size, 0);
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
			deref_r3(dlp, exitlbl, off, size, BPF_REG_0);

			/*
			 * The bio member of the request might be NULL.  In
			 * that case it is to be ignored.
			 */
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, exitlbl));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
		}

		goto done;
	}

	/*
	 * The non-XFS wait-start flavor already has the bio in arg0, so there
	 * is nothing left to be done.
	 */
	if (strcmp(prp->desc->prb, "wait-start") == 0)
		goto done;

	/* Handle the non-XFS wait-done flavor. */
	if (strcmp(prp->desc->prb, "wait-done") == 0) {
		/*
		 * We need to instrument submit_bio_wait(struct bio *):
		 *   - on entry, store bio in a TLS var
		 *   - on return, get bio and delete the TLS var
		 * We use a TLS var to distinguish among possible concurrent
		 * submit_bio_wait() on the CPU.
		 */
		dt_cg_tramp_decl_var(pcb, &v_biop);
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			dt_cg_tramp_get_var(pcb, "self->-io-bio-ptr", 1, BPF_REG_3);
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(0)));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_3, 0, BPF_REG_0));
			return 1;
		} else {
			dt_cg_tramp_get_var(pcb, "self->-io-bio-ptr", 0, BPF_REG_0);
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
			dt_cg_tramp_del_var(pcb, "self->-io-bio-ptr");
		}

	}

done:
	/*
	 * Note: DTrace does not currently support the use of fileinfo_t with
	 * io probes.  In Oracle Linux, there is no information about the file
	 * where the I/O request originated at the point where the io probes
	 * fire.
	 */
	emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(1), 0));

	return 0;
}

dt_provimpl_t	dt_io = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.populate	= &populate,
	.enable		= &dt_sdt_enable,
	.trampoline	= &trampoline,
	.probe_info	= &dt_sdt_probe_info,
	.destroy	= &dt_sdt_destroy,
};
