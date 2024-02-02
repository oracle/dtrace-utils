// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>
#define _QW_LOCKED		0x0ff

	.text
	.align	4
	.global	dt_rw_write_held
	.type	dt_rw_write_held, @function
dt_rw_write_held:
	/* dt_rw_write_held(tmp, rwlock) */

	/* %r6 = tmp */
	mov	%r6, %r1

	/* probe_read(tmp, 8, &rwlock->cnts) */
	lddw	%r3, RWLOCK_CNTS
	add	%r3, %r2
	mov	%r2, 8
	call	BPF_FUNC_probe_read

	/* %r0 = *tmp & _QW_LOCKED */
	ldxdw	%r0, [%r6+0]
	and	%r0, _QW_LOCKED
	exit
	.size	dt_rw_write_held, .-dt_rw_write_held
