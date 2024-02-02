// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>
#define MUTEX_FLAGS		0x07

	.text
	.align	4
	.global	dt_mutex_owned
	.type	dt_mutex_owned, @function
dt_mutex_owned:
	/* dt_mutex_owned(tmp, mutex) */

	/* %r6 = tmp */
	mov	%r6, %r1

	/* probe_read(tmp, 8, &mutex->owner) */
	lddw	%r3, MUTEX_OWNER
	add	%r3, %r2
	mov	%r2, 8
	call	BPF_FUNC_probe_read

	/* %r6 = *tmp & ~MUTEX_FLAGS */
	ldxdw	%r6, [%r6+0]
	and	%r6, ~MUTEX_FLAGS

	call	BPF_FUNC_get_current_task
	jeq	%r0, %r6, .Lowned
	mov	%r0, 0
	exit

.Lowned:
	mov	%r0, 1
	exit
	.size	dt_mutex_owned, .-dt_mutex_owned
