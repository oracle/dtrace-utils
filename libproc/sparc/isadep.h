/*
 * Underlying ISA-dependent functions for SPARC and SPARC64.
 */

/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Not protected against multiple inclusion: this file is included by
 * isadep_dispatch.h repeatedly, at local scope, to get different prototypes and
 * dispatch variables.
 */

#ifdef WANT_FIRST_ARG_DISPATCH

extern	uintptr_t Pread_first_arg_sparc64(struct ps_prochandle *P);
extern	uintptr_t Pread_first_arg_sparc32(struct ps_prochandle *P);

isa_dispatch_t dispatch[] = {
    {B_FALSE, EM_SPARCV9, (dispatch_fun_t *) Pread_first_arg_sparc32},
    {B_TRUE, EM_SPARCV9, (dispatch_fun_t *) Pread_first_arg_sparc64},
    {B_FALSE, EM_SPARC32PLUS, (dispatch_fun_t *) Pread_first_arg_sparc32},
    {B_TRUE, EM_SPARC32PLUS, (dispatch_fun_t *) Pread_first_arg_sparc64},
    {B_FALSE, EM_SPARC, (dispatch_fun_t *) Pread_first_arg_sparc32},
    {B_TRUE, EM_SPARC, (dispatch_fun_t *) Pread_first_arg_sparc64}, /* ??? */
    {0, 0, NULL}};

#endif

#ifdef WANT_GET_BKPT_IP

extern	long Pget_bkpt_ip_sparc64(struct ps_prochandle *P, int expect_esrch);

/*
 * I hope 32-on-64 traps are treated like 64-on-64 traps.
 */

isa_dispatch_t dispatch[] = {
    {B_FALSE, EM_SPARCV9, (dispatch_fun_t *) Pget_bkpt_ip_sparc64},
    {B_TRUE, EM_SPARCV9, (dispatch_fun_t *) Pget_bkpt_ip_sparc64},
    {B_FALSE, EM_SPARC32PLUS, (dispatch_fun_t *) Pget_bkpt_ip_sparc64},
    {B_TRUE, EM_SPARC32PLUS, (dispatch_fun_t *) Pget_bkpt_ip_sparc64},
    {B_FALSE, EM_SPARC, (dispatch_fun_t *) Pget_bkpt_ip_sparc64},
    {B_TRUE, EM_SPARC, (dispatch_fun_t *) Pget_bkpt_ip_sparc64}, /* ??? */
    {0, 0, NULL}};

#endif

#ifdef WANT_RESET_BKPT_IP

extern	long Preset_bkpt_ip_sparc(struct ps_prochandle *P, uintptr_t addr);

isa_dispatch_t dispatch[] = {
    {B_FALSE, EM_SPARCV9, (dispatch_fun_t *) Preset_bkpt_ip_sparc},
    {B_TRUE, EM_SPARCV9, (dispatch_fun_t *) Preset_bkpt_ip_sparc},
    {B_FALSE, EM_SPARC32PLUS, (dispatch_fun_t *) Preset_bkpt_ip_sparc},
    {B_TRUE, EM_SPARC32PLUS, (dispatch_fun_t *) Preset_bkpt_ip_sparc},
    {B_FALSE, EM_SPARC, (dispatch_fun_t *) Preset_bkpt_ip_sparc},
    {B_TRUE, EM_SPARC, (dispatch_fun_t *) Preset_bkpt_ip_sparc}, /* ??? */
    {0, 0, NULL}};

#endif

#ifdef WANT_GET_NEXT_IP

extern	long Pget_next_ip_sparc64(struct ps_prochandle *P, uintptr_t addr);

isa_dispatch_t dispatch[] = {
    {B_FALSE, EM_SPARCV9, (dispatch_fun_t *) Pget_next_ip_sparc64},
    {B_TRUE, EM_SPARCV9, (dispatch_fun_t *) Pget_next_ip_sparc64},
    {B_FALSE, EM_SPARC32PLUS, (dispatch_fun_t *) Pget_next_ip_sparc64},
    {B_TRUE, EM_SPARC32PLUS, (dispatch_fun_t *) Pget_next_ip_sparc64},
    {B_FALSE, EM_SPARC, (dispatch_fun_t *) Pget_next_ip_sparc64},
    {B_TRUE, EM_SPARC, (dispatch_fun_t *) Pget_next_ip_sparc64}, /* ??? */
    {0, 0, NULL}};

#endif
