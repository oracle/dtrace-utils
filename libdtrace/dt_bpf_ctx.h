/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef _DT_BPF_CTX_H
#define _DT_BPF_CTX_H

#include <dt_pt_regs.h>

/*
 * The DTrace BPF context.
 */
struct dt_bpf_context {
	uint32_t	epid;
	uint32_t	pad;
	uint64_t	fault;
	dt_pt_regs	regs;
	uint64_t	argv[10];
};

#define DCTX_EPID	offsetof(struct dt_bpf_context, epid)
#define DCTX_PAD	offsetof(struct dt_bpf_context, pad)
#define DCTX_FAULT	offsetof(struct dt_bpf_context, fault)
#define DCTX_REGS	offsetof(struct dt_bpf_context, regs)
#define DCTX_ARG(n)	offsetof(struct dt_bpf_context, argv[n])
#define DCTX_SIZE	sizeof(struct dt_bpf_context)

#endif /* _DT_BPF_CTX_H */
