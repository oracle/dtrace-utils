/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <dt_impl.h>
#include <dt_bpf_funcs.h>

const char *
dt_bpf_fname(int id)
{
	switch (id) {
	case DT_BPF_GET_GVAR:
		return "get_gvar";
	case DT_BPF_SET_GVAR:
		return "set_gvar";
	case DT_BPF_GET_TVAR:
		return "get_tvar";
	case DT_BPF_SET_TVAR:
		return "set_tvar";
#if 1 /* DEBUGGING */
	case DIF_OP_PUSHTR:
		return "pushtr";
	case DIF_OP_PUSHTV:
		return "pushtv";
#endif
	default:
		return "<unknown>";
	}
}
