// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>
#define MUTEX_FLAGS		0x07

	.text
	.align	4
	.global	dt_mutex_owner
	.type	dt_mutex_owner, @function
dt_mutex_owner:
	/* dt_mutex_owner(tmp, mutex) */

	/* %r6 = tmp */
	mov	%r6, %r1

	/* probe_read(tmp, 8, &mutex->owner) */
	lddw	%r3, MUTEX_OWNER
	add	%r3, %r2
	mov	%r2, 8
	call	BPF_FUNC_probe_read

	/* %r0 = *tmp & ~MUTEX_FLAGS */
	ldxdw	%r0, [%r6+0]
	and	%r0, ~MUTEX_FLAGS
	exit
	.size	dt_mutex_owner, .-dt_mutex_owner
