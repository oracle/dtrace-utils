/*
 * Underlying ISA-dependent functions for SPARC and SPARC64.
 */

/*
 * Oracle Linux DTrace.
 * Copyright © 2013, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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

extern uintptr_t Pget_bkpt_ip_sparc64(struct ps_prochandle *P, int expect_esrch);

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
