/*
 * Underlying ISA-dependent functions for x86 and x86-64.
 */

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Not protected against multiple inclusion: this file is included by
 * isadep_dispatch.h repeatedly, at local scope, to get different prototypes and
 * dispatch variables.
 */

#ifdef WANT_FIRST_ARG_DISPATCH

extern	uintptr_t Pread_first_arg_x86_64(struct ps_prochandle *P);
extern	uintptr_t Pread_first_arg_x86(struct ps_prochandle *P);

isa_dispatch_t dispatch[] = {
    {B_TRUE, EM_X86_64, (dispatch_fun_t *) Pread_first_arg_x86_64},
    {B_FALSE, EM_386, (dispatch_fun_t *) Pread_first_arg_x86},
    {0, 0, NULL}};

#endif

#ifdef WANT_GET_BKPT_IP

extern	uintptr_t Pget_bkpt_ip_x86(struct ps_prochandle *P, int expect_esrch);

isa_dispatch_t dispatch[] = {
    {B_TRUE, EM_X86_64, (dispatch_fun_t *) Pget_bkpt_ip_x86},
    {B_FALSE, EM_386, (dispatch_fun_t *) Pget_bkpt_ip_x86},
    {0, 0, NULL}};

#endif

#ifdef WANT_RESET_BKPT_IP

extern	long Preset_bkpt_ip_x86(struct ps_prochandle *P, uintptr_t addr);

isa_dispatch_t dispatch[] = {
    {B_TRUE, EM_X86_64, (dispatch_fun_t *) Preset_bkpt_ip_x86},
    {B_FALSE, EM_386, (dispatch_fun_t *) Preset_bkpt_ip_x86},
    {0, 0, NULL}};

#endif

#ifdef WANT_GET_NEXT_IP

#error get_next_ip() is not implemented on this platform.

#endif
