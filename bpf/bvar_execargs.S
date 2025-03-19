// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>
#include <dtrace/faults_defines.h>

/*
 * uint64_t dt_bvar_execargs(const dt_dctx_t *dctx, char *args)
 */
	.text
	.align	4
	.global	dt_bvar_execargs
	.type	dt_bvar_execargs, @function
dt_bvar_execargs:
	stxdw	[%fp+-8], %r1			/* save dctx to stack */
	mov	%r9, %r2			/* %r9 = args */

	mov	%r6, 0				/* set %r6 in case of error */
	call	BPF_FUNC_get_current_task	/* get curthread (T) */
	jeq	%r0, 0, .Lerror

	mov	%r6, %r0
	add	%r6, TASK_MM			/* %r6 = &(T->mm) */

	mov	%r1, %r9
	mov	%r2, 8
	mov	%r3, %r6
	call	BPF_FUNC_probe_read
	jne	%r0, 0, .Lerror

	ldxdw	%r8, [%r9+0]			/* %r8 = T->mm */
	jeq	%r8, 0, .Lempty
	mov	%r6, %r8
	add	%r6, TASK_MM_ARG_START		/* ptr = &(T->mm->arg_start) */

	mov	%r1, %r9
	mov	%r2, 8
	mov	%r3, %r6
	call	BPF_FUNC_probe_read
	jne	%r0, 0, .Lerror

	ldxdw	%r7, [%r9+0]			/* %r7 = T->mm->arg_start */
	mov	%r6, %r8
	add	%r6, TASK_MM_ARG_END		/* ptr = &(T->mm->arg_end) */

	mov	%r1, %r9
	mov	%r2, 8
	mov	%r3, %r6
	call	BPF_FUNC_probe_read
	jne	%r0, 0, .Lerror

	ldxdw	%r6, [%r9+0]			/* %r6 = T->mm->arg_end */

	mov	%r8, %r6
	sub	%r8, %r7			/* %r8 = len = arg_end - arg_start */
	jslt	%r8, 2, .Lempty
	mov	%r0, STRSZ
	jslt	%r8, %r0, .Llen_ok
	mov	%r8, %r0
.Llen_ok:

	/* read data from arg_start to arg_end */
	mov	%r1, %r9
	mov	%r2, %r8
	mov	%r3, %r7
	call	BPF_FUNC_probe_read_user	/* bpf_probe_read_user(&args, len + 1, arg_start) */
	jne	%r0, 0, .Lerror

	/* loop over args and replace '\0' with ' ' */
	mov	%r1, %r8
	sub	%r1, 2
.Lloop:
	mov	%r2, %r9
	add	%r2, %r1
	ldxb	%r0, [%r2+0]
	jne	%r0, 0, .Lnot_nil
	stb	[%r2+0], 32
.Lnot_nil:
	sub	%r1, 1
	jsge	%r1, 0, .Lloop

.Ldone:
	mov	%r0, %r9
	exit					/* return args */
.Lerror:
	ldxdw	%r1, [%fp+-8]
	mov	%r2, PC
	mov	%r3, DTRACEFLT_BADADDR
	mov	%r4, %r6
	call	dt_probe_error
.Lempty:
	mov	%r0, %r9
	stb	[%r9+0], 0			/* args[0] = 0 */
	exit					/* return args */
	.size	dt_bvar_execargs, .-dt_bvar_execargs
