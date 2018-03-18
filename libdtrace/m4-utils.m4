/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* quote(args) - convert args to single-quoted string */
m4_define([[m4_quote]], [[m4_ifelse([[$#]], [[0]], [[]], [[[[$*]]]])]])

/* m4_dquote(args) - convert args to quoted list of quoted strings */
m4_define([[m4_dquote]], [[[[$@]]]])

/*
 * m4_foreachq(x, `item_1, item_2, ..., item_n', stmt)
 *  quoted list, improved version
 */
m4_define([[m4_foreachq]], [[m4_pushdef([[$1]])_$0($@)m4_popdef([[$1]])]])
m4_define([[_m4_arg1q]], [[[[$1]]]])
m4_define([[_m4_rest]], [[m4_ifelse([[$#]], [[1]], [[]], [[m4_dquote(m4_shift($@))]])]])
m4_define([[_m4_foreachq]], [[m4_ifelse([[$2]], [[]], [[]],
  [[m4_define([[$1]], _m4_arg1q($2))$3[[]]$0([[$1]], _m4_rest($2), [[$3]])]])]])

/*
 * m4_kernelver(v1, v2, v3) - convers kernel versioning to single value used in macros.
 */
m4_define([[m4_kver]], [[m4_eval([[$1 * 1000 * 1000 + $2 * 1000 + $3]])]])

/*
 * To prevent M4 argument expansion, quoting must be applied between $ and
 * argument number - $[[]]1.  The dtrace_for_kernel performs multiple expansions,
 * so it requires addition of more quoting. This is required to support usage
 * of $1 in the values in the list of kernels.  The m4_argq macro is a helper.
 * Instead of $1 you are supossed to use m4_argq(1).
 *
 * This does not work for every use case.  When a value depends on result of
 * another dtrace_for_kernel macro it may require more quotes.  The second
 * argument allows adding more quotes when required.
 */
m4_define([[m4_argq]], $[[[[[[[[[[$2]]]]]]]]]]$1)
