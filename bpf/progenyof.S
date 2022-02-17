// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 */

#define BPF_FUNC_probe_read	4
#define BPF_FUNC_get_current_task	35

	.text
	.align	4
	.global	dt_progenyof
	.type	dt_progenyof, @function
dt_progenyof:
#define ARG %r6
#define CNT %r7
#define PTR %r8
#define VAL %r9

	/* uint64_t dt_progenyof(pid_t arg); */
	mov	ARG, %r1
	lsh	ARG, 32
	rsh	ARG, 32

	/* assure the BPF verifier there is no infinite loop */
	mov	CNT, 256

	/* ptr = bpf_get_current_task() */
	call	BPF_FUNC_get_current_task
	mov	PTR, %r0

.Lloop:
	/* if (ptr == 0) goto Lret0 */
	jeq	PTR, 0, .Lret0

	/* if (count <= 0) goto Lret0 */
	jsle	CNT, 0, .Lret0

	/* val = *((uint32_t *)(ptr + TASK_PID)), using [%fp+-8] as temp */
	mov	%r1, %fp
	add	%r1, -8
	mov	%r2, 4
	lddw	%r3, TASK_PID
	add	%r3, PTR
	call	BPF_FUNC_probe_read
	jne	%r0, 0, .Lret0
	ldxw	VAL, [%fp+-8]
	lsh	VAL, 32
	rsh	VAL, 32

	/* if (val == arg) goto Lret1 */
	jeq	VAL, ARG, .Lret1

	/* val = *((uint64_t *)(ptr + TASK_REAL_PARENT)), using [%fp+-8] as temp */
	mov	%r1, %fp
	add	%r1, -8
	mov	%r2, 8
	lddw	%r3, TASK_REAL_PARENT
	add	%r3, PTR
	call	BPF_FUNC_probe_read
	jne	%r0, 0, .Lret0
	ldxdw	VAL, [%fp+-8]

	/* if (val == ptr) goto Lret0 */
	jeq	VAL, PTR, .Lret0

	/* ptr = val */
	mov	PTR, VAL

	/* count-- */
	sub	CNT, 1

	/* goto Lloop */
	ja	.Lloop

.Lret0:
	/* return 0 */
	mov	%r0, 0
	exit

.Lret1:
	/* return 1 */
	mov	%r0, 1
	exit
	.size	dt_progenyof, .-dt_progenyof
#undef ARG
#undef CNT
#undef PTR
#undef VAL
