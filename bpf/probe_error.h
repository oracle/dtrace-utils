// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
 */
#ifndef BPF_PROBE_ERRROR_H
#define BPF_PROBE_ERRROR_H

#include <stdint.h>
#include <dt_dctx.h>

extern void dt_probe_error(const dt_dctx_t *dctx, uint64_t pc, uint64_t fault,
			   uint64_t illval);

#endif /* BPF_PROBE_ERRROR_H */
