/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _DT_BPF_BUILTINS_H
#define _DT_BPF_BUILTINS_H

#ifdef  __cplusplus
extern "C" {
#endif

#define DT_BPF_MAP_BUILTINS(FN) \
	FN(GET_GVAR, get_gvar), \
	FN(GET_STRING, get_string), \
	FN(GET_TVAR, get_tvar), \
	FN(MEMCPY, memcpy), \
	FN(PREDICATE, predicate), \
	FN(PROGRAM, program), \
	FN(SET_GVAR, set_gvar), \
	FN(SET_TVAR, set_tvar), \
	FN(STRNLEN, strnlen)

#define DT_BPF_ENUM_FN(x, y)	DT_BPF_ ## x
enum dt_bpf_builtin_ids {
        DT_BPF_MAP_BUILTINS(DT_BPF_ENUM_FN),
        DT_BPF_LAST_ID,
};
#undef DT_BPF_ENUM_FN

typedef struct dt_bpf_func	dt_bpf_func_t;
typedef struct dt_bpf_builtin	dt_bpf_builtin_t;
struct dt_bpf_builtin {
	const char	*name;
	dt_bpf_func_t	*sym;
};

extern dt_bpf_builtin_t		dt_bpf_builtins[];

#ifdef  __cplusplus
}
#endif

#endif /* _DT_BPF_FUNCS_H */
